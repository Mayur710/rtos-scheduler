/*
 * Reproduces the "before vs after aging" table in the README.
 *
 * Runs the REAL task set (SensorReadTask / LogTask / BlinkTask) twice over
 * 1000 ticks:
 *
 *   Scenario A: agingRatePerTick = 0  -> pure fixed priority (the original
 *              starvation bug): the p=0 task takes every tick.
 *   Scenario B: agingRatePerTick = 1  -> the shipped default.
 *
 * The scheduler's per-tick trace is captured and parsed, so this uses the
 * production code path end-to-end (HAL mocks included) rather than a model.
 *
 * Exit code 0 = measured numbers match the README.
 *
 * Build:  g++ -std=c++17 -Wall -Wextra -Wpedantic -Iinclude \
 *             src/Task.cpp src/Scheduler.cpp tools/compare_aging.cpp
 */
#include "HAL/MockI2C.h"
#include "HAL/MockUART.h"
#include "Scheduler.h"
#include "tasks/BlinkTask.h"
#include "tasks/LogTask.h"
#include "tasks/SensorReadTask.h"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace {

const int kTicks = 1000;
const char* kTaskOrder[] = {"SensorReadTask", "LogTask", "BlinkTask"};

struct Selection {
    int tick;
    std::string task;
};

struct Row {
    int runs = 0;
    int maxSkipped = 0;
    int firstRunTick = -1;
    int lastRunTick = -1;
};

// Runs one scenario and parses "[Tick N] Scheduler: Running Task(Name, ..."
// out of the captured trace.
std::vector<Selection> runScenario(int agingRatePerTick) {
    MockUART uart;
    MockI2C i2c;
    SensorReadTask sensorTask(1, 0, i2c, 0x48);
    LogTask logTask(2, 1, uart);
    BlinkTask blinkTask(3, 2, 13);

    Scheduler scheduler(agingRatePerTick);
    scheduler.addTask(&sensorTask);
    scheduler.addTask(&logTask);
    scheduler.addTask(&blinkTask);

    std::ostringstream captured;
    std::streambuf* saved = std::cout.rdbuf(captured.rdbuf());
    scheduler.run(kTicks);
    std::cout.rdbuf(saved);

    std::vector<Selection> out;
    std::istringstream in(captured.str());
    std::string line;
    while (std::getline(in, line)) {
        if (line.compare(0, 6, "[Tick ") != 0) continue;
        const size_t tickEnd = line.find(']');
        if (tickEnd == std::string::npos) continue;
        const int tick = std::stoi(line.substr(6, tickEnd - 6));

        const size_t marker = line.find("Running Task(", tickEnd);
        if (marker == std::string::npos) continue;
        const size_t begin = marker + 13;
        const size_t end = line.find(',', begin);
        if (end == std::string::npos) continue;

        Selection s;
        s.tick = tick;
        s.task = line.substr(begin, end - begin);
        out.push_back(s);
    }
    return out;
}

std::map<std::string, Row> summarize(const std::vector<Selection>& sel) {
    std::map<std::string, Row> rows;
    for (size_t i = 0; i < sel.size(); ++i) {
        Row& r = rows[sel[i].task];
        if (r.runs > 0) {
            r.maxSkipped = std::max(r.maxSkipped, sel[i].tick - r.lastRunTick - 1);
        } else {
            r.firstRunTick = sel[i].tick;
        }
        ++r.runs;
        r.lastRunTick = sel[i].tick;
    }
    return rows;
}

void printScenario(const std::string& title, std::map<std::string, Row>& rows) {
    // MockI2C leaves std::setfill('0') behind on the shared stream, so reset it.
    std::cout << std::setfill(' ');
    std::cout << "\n=== " << title << " ===\n";
    for (size_t i = 0; i < 3; ++i) {
        const std::string name = kTaskOrder[i];
        const Row& r = rows[name];  // default-constructs 0 if it never ran
        std::cout << "  " << std::left << std::setw(16) << name
                  << " runs=" << std::right << std::setw(4) << r.runs;
        if (r.runs == 0) {
            std::cout << "  (STARVED: 0 of " << kTicks << " ticks)\n";
        } else {
            std::cout << "  (" << std::fixed << std::setprecision(1)
                      << (100.0 * r.runs / kTicks) << "%)"
                      << "  max ticks skipped=" << r.maxSkipped
                      << "  first run=tick " << r.firstRunTick << "\n";
        }
    }
}

}  // namespace

int main() {
    std::cout << "Starvation before/after comparison -- " << kTicks
              << " ticks, 3 tasks (priorities 0/1/2)\n";

    std::vector<Selection> beforeSel = runScenario(0);
    std::vector<Selection> afterSel = runScenario(1);

    std::map<std::string, Row> before = summarize(beforeSel);
    std::map<std::string, Row> after = summarize(afterSel);

    printScenario("BEFORE: agingRatePerTick = 0 (pure fixed priority)", before);
    printScenario("AFTER : agingRatePerTick = 1 (default, linear aging)", after);

    bool ok = true;
    ok = ok && beforeSel.size() == static_cast<size_t>(kTicks);
    ok = ok && afterSel.size() == static_cast<size_t>(kTicks);
    ok = ok && before["SensorReadTask"].runs == kTicks;
    ok = ok && before["LogTask"].runs == 0;
    ok = ok && before["BlinkTask"].runs == 0;
    ok = ok && after["SensorReadTask"].runs == 501;
    ok = ok && after["LogTask"].runs == 250;
    ok = ok && after["BlinkTask"].runs == 249;
    ok = ok && after["LogTask"].maxSkipped <= 3;
    ok = ok && after["BlinkTask"].maxSkipped <= 3;
    ok = ok && after["SensorReadTask"].maxSkipped <= 1;

    std::cout << "\nExpected: 1000/0/0 without aging, 501/250/249 with aging,"
                 " worst-case skip <= 3 ticks\n";
    if (!ok) {
        std::cerr << "VERDICT: FAIL (numbers do not match the README)\n";
        return 1;
    }
    std::cout << "VERDICT: PASS (starvation reproduced and eliminated)\n";
    return 0;
}
