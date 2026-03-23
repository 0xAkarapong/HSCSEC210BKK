import { YahooFinance } from "yahoo-finance2";
import { readFileSync, writeFileSync, existsSync } from "fs";

const DATA_FILE = "/var/lib/nvda-tracker/data.json";
const SYMBOL = "NVDA";

interface DayData {
  date: string;
  opening: number | null;
  closing: number | null;
  min: number | null;
  max: number | null;
  lastPrice: number | null;
  lastTime: string | null;
}

function getEasternTime(): { date: string; time: number; hour: number; minute: number } {
  const now = new Date();
  const etOffset = -4 * 60;
  const utc = now.getTime() + now.getTimezoneOffset() * 60000;
  const et = new Date(utc + etOffset * 60000);
  const hours = et.getHours();
  const minutes = et.getMinutes();
  const time = hours * 60 + minutes;
  const date = et.toISOString().split("T")[0];
  return { date, time, hour: hours, minute: minutes };
}

function isTradingHours(time: number): boolean {
  return time >= 570 && time < 960;
}

function isEOD(time: number): boolean {
  return time >= 955 && time < 960;
}

function loadData(): DayData {
  if (existsSync(DATA_FILE)) {
    try {
      const raw = readFileSync(DATA_FILE, "utf-8");
      return JSON.parse(raw);
    } catch {
      return { date: "", opening: null, closing: null, min: null, max: null, lastPrice: null, lastTime: null };
    }
  }
  return { date: "", opening: null, closing: null, min: null, max: null, lastPrice: null, lastTime: null };
}

function saveData(data: DayData): void {
  writeFileSync(DATA_FILE, JSON.stringify(data, null, 2));
}

async function getPrice(): Promise<number | null> {
  try {
    const quote = await YahooFinance.quote(SYMBOL);
    return quote.regularMarketPrice ?? null;
  } catch {
    return null;
  }
}

async function printEOD(data: DayData): Promise<void> {
  if (data.opening !== null && data.closing !== null && data.min !== null && data.max !== null) {
    console.log(`\n=== EOD Summary for ${data.date} ===`);
    console.log(`Opening: $${data.opening.toFixed(2)}`);
    console.log(`Closing: $${data.closing.toFixed(2)}`);
    console.log(`Min: $${data.min.toFixed(2)}`);
    console.log(`Max: $${data.max.toFixed(2)}`);
    console.log("=================================\n");
  }
  writeFileSync(DATA_FILE, JSON.stringify({ date: "", opening: null, closing: null, min: null, max: null, lastPrice: null, lastTime: null }));
}

async function main(): Promise<void> {
  const { date, time, hour, minute } = getEasternTime();
  let data = loadData();

  if (data.date !== date) {
    if (data.date !== "" && data.closing === null) {
      await printEOD(data);
    }
    data = { date, opening: null, closing: null, min: null, max: null, lastPrice: null, lastTime: null };
  }

  if (!isTradingHours(time)) {
    if (time >= 960) {
      await printEOD(data);
    }
    const nextRun = 60000 - (minute * 1000) % 60000;
    setTimeout(main, nextRun);
    return;
  }

  const price = await getPrice();

  if (price !== null) {
    if (data.opening === null) {
      data.opening = price;
      console.log(`[${date}] NVDA opened at $${price.toFixed(2)}`);
    }

    if (data.lastPrice !== null) {
      const diff = price - data.lastPrice;
      const sign = diff >= 0 ? "+" : "";
      console.log(`[${date} ${hour.toString().padStart(2, "0")}:${minute.toString().padStart(2, "0")}] NVDA: $${price.toFixed(2)} (${sign}${diff.toFixed(2)})`);
    } else {
      console.log(`[${date} ${hour.toString().padStart(2, "0")}:${minute.toString().padStart(2, "0")}] NVDA: $${price.toFixed(2)}`);
    }

    data.lastPrice = price;
    data.lastTime = new Date().toISOString();

    if (data.min === null || price < data.min) data.min = price;
    if (data.max === null || price > data.max) data.max = price;

    if (isEOD(time)) {
      data.closing = price;
      await printEOD(data);
      return;
    }
  }

  saveData(data);
  setTimeout(main, 60000);
}

main();
