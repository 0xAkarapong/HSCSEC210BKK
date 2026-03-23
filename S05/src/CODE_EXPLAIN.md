# Code Explanation

## Overview

The program tracks NVDA (Nvidia) stock price during NASDAQ trading hours and prints price updates every minute.

## Key Components

### 1. Data Structure (`DayData`)
```typescript
interface DayData {
  date: string;       // Current trading date (YYYY-MM-DD)
  opening: number;   // Opening price
  closing: number;    // Closing price
  min: number;       // Minimum price of the day
  max: number;       // Maximum price of the day
  lastPrice: number; // Previous minute's price (for diff calculation)
  lastTime: string;  // Timestamp of last update
}
```

### 2. Time Handling
- **Eastern Time**: NASDAQ operates in ET (UTC-4 during daylight saving)
- **Trading Hours**: 9:30 AM - 4:00 PM ET (570-960 minutes from midnight)
- **EOD Window**: 3:55 PM - 4:00 PM ET (955-960 minutes)

### 3. Persistence Strategy (Restart Survival)

The program writes to `/var/lib/nvda-tracker/data.json` every minute.

**On startup:**
1. Load data from file
2. Check if stored `date` matches today
3. If different date AND previous day has no closing → print EOD for previous day
4. If same date → continue tracking with existing opening/min/max values

This ensures the EOD summary is printed even if the program was restarted during the day.

## How Restart Survival Was Achieved

### The Problem
The assignment requires the program to survive being stopped/restarted during the day AND still print EOD information. This is challenging because:
- If the program stops, we lose all in-memory data
- If restarted, we need to know what happened before the restart

### The Solution: Persistent Storage

**1. Every Minute: Save State**
```typescript
function saveData(data: DayData): void {
  writeFileSync(DATA_FILE, JSON.stringify(data, null, 2));
}
```
After each price update, the entire `DayData` object is written to `/var/lib/nvda-tracker/data.json`.

**2. On Startup: Check Date**
```typescript
let data = loadData();
if (data.date !== date) {
  if (data.date !== "" && data.closing === null) {
    await printEOD(data);  // Print EOD for previous day!
  }
  data = { date, ... };  // Start fresh for new day
}
```

**3. If Same Day: Resume Tracking**
If the stored date matches today, the program loads:
- `opening` - continues from where it left off
- `min` / `max` - preserves the day's lowest/highest prices
- `lastPrice` - can still calculate diff from previous

### Key Scenarios

| Scenario | What Happens |
|----------|-------------|
| Restart at 2pm | Loads opening/min/max from file, continues tracking |
| Restart at 4:05pm | If previous run had no closing, prints EOD from saved data |
| First run of day | Starts fresh, sets opening when first price arrives |

### Why This Works
- Data is saved BEFORE any output (line 123: `saveData` after `getPrice`)
- Even if killed at 3:59pm, the 3:58pm data is already persisted
- The EOD check runs on EVERY startup, catching missed closes from previous sessions

### 4. Main Loop Flow

```
main()
  ├─ Get current ET time
  ├─ Load persisted data
  ├─ If new day:
  │   └─ If previous day had no closing → print EOD
  │   └─ Reset data for new day
  ├─ If outside trading hours:
  │   └─ Print EOD if after 4 PM ET
  │   └─ Wait until next minute, retry
  ├─ Fetch current price from Yahoo Finance
  ├─ If price available:
  │   ├─ Set opening price if first trade
  │   ├─ Print price + diff from previous
  │   ├─ Update min/max
  │   └─ If EOD time → print EOD summary
  ├─ Save data to file
  └─ Wait 1 minute, repeat
```

### 5. Output Format

**Regular update:**
```
[2026-03-23 14:30] NVDA: $145.50 (+1.25)
```

**EOD summary:**
```
=== EOD Summary for 2026-03-23 ===
Opening: $144.00
Closing: $145.50
Min: $143.20
Max: $146.80
=================================
```

## Files

| File | Purpose |
|------|---------|
| `nvda-tracker.ts` | Main program |
| `package.json` | Dependencies (yahoo-finance2) |
| `nvda-tracker.service` | Systemd unit file |
| `/var/lib/nvda-tracker/data.json` | Persisted daily data |