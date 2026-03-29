#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

void *manager(void*);
void *miner(void*);

uint64_t solution = 0;
int done = 0;

typedef struct {
   uint64_t items[10];
   size_t length;
   pthread_mutex_t mutex;
   pthread_cond_t can_add;
   pthread_cond_t can_pop;
} queue_t;

void queue_init(queue_t *this) {
   this->length = 0;
   pthread_mutex_init(&this->mutex, NULL);
   pthread_cond_init(&this->can_add, NULL);
   pthread_cond_init(&this->can_pop, NULL);
}

int queue_can_add(queue_t *this) {
   return this->length + 1 < 10;
}

void queue_add(queue_t *this, uint64_t item) {
   pthread_mutex_lock(&this->mutex);
   while (!queue_can_add(this) && !done) {
      pthread_cond_wait(&this->can_add, &this->mutex);
   }
   if (done) {
      pthread_mutex_unlock(&this->mutex);
      return;
   }
   this->items[this->length] = item;
   this->length++;
   pthread_cond_signal(&this->can_pop);
   pthread_mutex_unlock(&this->mutex);
}

int queue_can_pop(queue_t *this) {
   return this->length > 0;
}

uint64_t queue_pop(queue_t *this) {
   pthread_mutex_lock(&this->mutex);
   while (!queue_can_pop(this) && !done) {
      pthread_cond_wait(&this->can_pop, &this->mutex);
   }
   if (done) {
      pthread_mutex_unlock(&this->mutex);
      return 0;
   }
   int result = this->items[0];
   this->length--;
   for (size_t i = 0; i < this->length; i++) {
      this->items[i] = this->items[i+1];
   }
   pthread_cond_signal(&this->can_add);
   pthread_mutex_unlock(&this->mutex);
   return result;
}

uint64_t hash(uint64_t x) {
    x = (x ^ (x >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
    x = (x ^ (x >> 27)) * UINT64_C(0x94d049bb133111eb);
    x = x ^ (x >> 31);
    return x;
}

const int N_MINERS = 100;

const uint64_t SLICE_SIZE = 1000000;
const uint64_t LOWER_BITS_MASK = 0xfffffff;

uint64_t seed;

queue_t queue;

int main() {
   pthread_t thread_manager;
   pthread_t threads_miners[N_MINERS];

   srandom(time(NULL));
   seed = random();

   queue_init(&queue);

   assert(!pthread_create(&thread_manager, NULL, &manager, NULL));
   for (int i = 0; i < N_MINERS; i++) {
      assert(!pthread_create(&threads_miners[i], NULL, &miner, NULL));
   }
   assert(!pthread_join(thread_manager, NULL));
   int sum = 0;
   for (int i = 0; i < N_MINERS; i++) {
      assert(!pthread_join(threads_miners[i], NULL));
   }
   printf("%d\n", sum);

   exit(0);
}

void *manager(void* _param) {
   uint64_t slice_base = SLICE_SIZE;
   while (!solution) {
      queue_add(&queue, slice_base);
      if (solution) break;
      printf("sent %llu\n", slice_base);
      slice_base += SLICE_SIZE;
   }
   printf("manager sees solution %llu\n", solution);
   pthread_mutex_lock(&queue.mutex);
   done = 1;
   pthread_cond_broadcast(&queue.can_add);
   pthread_cond_broadcast(&queue.can_pop);
   pthread_mutex_unlock(&queue.mutex);
   return NULL;
}

void *miner(void* _param) {
   while (!solution && !done) {
      uint64_t slice_base = queue_pop(&queue);
      if (done) return NULL;
      for (uint64_t i = slice_base; i < slice_base + SLICE_SIZE; i++) {
         if (solution || done) break;
         uint64_t hashed = i ^ seed;
         for (int j = 0; j < 10; j++) {
            hashed = hash(hashed);
         }
         if ((hashed & LOWER_BITS_MASK) == 0) {
            solution = i;
            printf("miner found solution %llu\n", i);
            pthread_mutex_lock(&queue.mutex);
            done = 1;
            pthread_cond_broadcast(&queue.can_add);
            pthread_cond_broadcast(&queue.can_pop);
            pthread_mutex_unlock(&queue.mutex);
            return NULL;
         }
      }
   }
   return NULL;
}
