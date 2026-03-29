import Foundation
import Darwin

let N_MINERS = 100
let SLICE_SIZE: UInt64 = 1000000
let LOWER_BITS_MASK: UInt64 = 0xfffffff

var solution: UInt64 = 0
var done: Int32 = 0
var seed: UInt64 = 0

var queue: UnsafeMutablePointer<UInt64>? = nil
var queueLen: Int = 0
let queueCap = 10

var queueMutex = pthread_mutex_t()
var canAdd = pthread_cond_t()
var canPop = pthread_cond_t()

func queueInit() {
    queue = UnsafeMutablePointer<UInt64>.allocate(capacity: queueCap)
    queueLen = 0
    pthread_mutex_init(&queueMutex, nil)
    pthread_cond_init(&canAdd, nil)
    pthread_cond_init(&canPop, nil)
}

func queueAdd(_ item: UInt64) {
    pthread_mutex_lock(&queueMutex)
    while queueLen >= queueCap && done == 0 {
        pthread_cond_wait(&canAdd, &queueMutex)
    }
    if done == 0 {
        queue?[queueLen] = item
        queueLen += 1
        pthread_cond_signal(&canPop)
    }
    pthread_mutex_unlock(&queueMutex)
}

func queuePop() -> UInt64 {
    pthread_mutex_lock(&queueMutex)
    while queueLen == 0 && done == 0 {
        pthread_cond_wait(&canPop, &queueMutex)
    }
    if done == 0 {
        let result = queue?[0] ?? 0
        for i in 0..<(queueLen - 1) {
            queue?[i] = queue?[i + 1] ?? 0
        }
        queueLen -= 1
        pthread_cond_signal(&canAdd)
        pthread_mutex_unlock(&queueMutex)
        return result
    }
    pthread_mutex_unlock(&queueMutex)
    return 0
}

func myHash(_ x: UInt64) -> UInt64 {
    var x = x
    x = (x ^ (x >> 30)) &* 0xbf58476d1ce4e5b9
    x = (x ^ (x >> 27)) &* 0x94d049bb133111eb
    x = x ^ (x >> 31)
    return x
}

func manager() {
    var sliceBase: UInt64 = SLICE_SIZE
    while solution == 0 {
        queueAdd(sliceBase)
        if solution != 0 { break }
        print("sent \(sliceBase)")
        fflush(stdout)
        sliceBase += SLICE_SIZE
    }
    print("manager sees solution \(solution)")
    fflush(stdout)
    pthread_mutex_lock(&queueMutex)
    done = 1
    pthread_cond_broadcast(&canAdd)
    pthread_cond_broadcast(&canPop)
    pthread_mutex_unlock(&queueMutex)
}

func miner(_ id: UnsafeMutableRawPointer?) {
    while solution == 0 && done == 0 {
        let sliceBase = queuePop()
        if done == 1 { return }
        
        var i = sliceBase
        while i < sliceBase + SLICE_SIZE {
            if solution != 0 || done == 1 { break }
            var hashed = i ^ seed
            for _ in 0..<10 {
                hashed = myHash(hashed)
            }
            if (hashed & LOWER_BITS_MASK) == 0 {
                solution = i
                print("miner found solution \(i)")
                fflush(stdout)
                pthread_mutex_lock(&queueMutex)
                done = 1
                pthread_cond_broadcast(&canAdd)
                pthread_cond_broadcast(&canPop)
                pthread_mutex_unlock(&queueMutex)
                return
            }
            i += 1
        }
    }
}

srand48(12345)
seed = UInt64(drand48() * Double(UInt64.max))

queueInit()

var managerThread: pthread_t? = nil
pthread_create(&managerThread, nil, { _ in
    manager()
    return nil
}, nil)

var minerThreads: [pthread_t?] = []
for i in 0..<N_MINERS {
    var tid: pthread_t? = nil
    pthread_create(&tid, nil, { ptr in
        miner(ptr)
        return nil
    }, UnsafeMutableRawPointer(bitPattern: i))
    minerThreads.append(tid)
}

pthread_join(managerThread!, nil)
for tid in minerThreads {
    pthread_join(tid!, nil)
}

print("0")
fflush(stdout)
