#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

using namespace std;

namespace hnp_new {

enum Symbol {
    HV1, HV2, HV3, HV4, LV1, LV2, LV3, LV4,
    WILD, HARD_HAT, HAZARD_HARD_HAT, HORIZONTAL_SAW_HARD_HAT,
    VERTICAL_SAW_HARD_HAT, SUPER_SAW_HARD_HAT, PIG1, PIG2, PIG3,
    TOOLBOX, TAPE, BLANK
};

enum GameState {
    BASE_GAME,
    NORMAL_FREE_SPINS,
    SUPER_SAW_FREE_SPINS
};

enum Frame {
    NO_FRAME,
    STRAW_FRAME,
    STICK_FRAME,
    BRICK_FRAME
};

constexpr int NO_OF_REELS = 5;
constexpr int NO_OF_ROWS = 6;
constexpr int BET = 20;
constexpr int MAX_WIN_XBET = 10000;
constexpr long long MAX_WIN_CREDITS = MAX_WIN_XBET * BET;
constexpr uint64_t DEFAULT_SPINS = 100000;
constexpr uint64_t DEV_BENCH_SPINS = 1000000;
constexpr unsigned int DEFAULT_SEED = 123456789;

using Window = array<array<Symbol, NO_OF_REELS>, NO_OF_ROWS>;
using FrameWindow = array<array<Frame, NO_OF_REELS>, NO_OF_ROWS>;

struct WeightedInt {
    int value;
    int weight;
};

struct WeightedSymbol {
    Symbol value;
    int weight;
};

struct WeightedDouble {
    double value;
    int weight;
};

struct ReelSet {
    GameState state = BASE_GAME;
    int active_rows = 3;
    array<array<Symbol, 100>, NO_OF_REELS> reels{};
};

struct SpinResult {
    Window window{};
    long long base_win = 0;
    long long feature_win = 0;
    long long overflow_win = 0;
    long long jackpot_win = 0;
    int active_rows = 3;
    bool triggered_normal_fs = false;
    bool triggered_super_fs = false;
    int hard_hat_count = 0;
};

struct FeatureResult {
    long long total_win = 0;
    long long overflow_win = 0;
    long long jackpot_win = 0;
    int spins_played = 0;
    int retriggers = 0;
};

struct Statistics {
    uint64_t spins = 0;
    long long total_bet = 0;
    long long total_win = 0;
    long long base_ways_win = 0;
    long long normal_fs_win = 0;
    long long super_saw_fs_win = 0;
    long long overflow_win = 0;
    long long jackpot_win = 0;
    uint64_t hits = 0;
    uint64_t base_hits = 0;
    uint64_t normal_fs_triggers = 0;
    uint64_t super_saw_fs_triggers = 0;
    uint64_t girder_triggers = 0;
    uint64_t mystery_stack_triggers = 0;
    uint64_t retriggers = 0;
    uint64_t feature_spins = 0;
    long long max_win = 0;
    long double win_sq_sum = 0.0L;
    array<uint64_t, 4> reelset_usage_base{};
    array<uint64_t, 4> reelset_usage_normal{};
    array<uint64_t, 4> reelset_usage_super{};

    void merge(const Statistics& other) {
        spins += other.spins;
        total_bet += other.total_bet;
        total_win += other.total_win;
        base_ways_win += other.base_ways_win;
        normal_fs_win += other.normal_fs_win;
        super_saw_fs_win += other.super_saw_fs_win;
        overflow_win += other.overflow_win;
        jackpot_win += other.jackpot_win;
        hits += other.hits;
        base_hits += other.base_hits;
        normal_fs_triggers += other.normal_fs_triggers;
        super_saw_fs_triggers += other.super_saw_fs_triggers;
        girder_triggers += other.girder_triggers;
        mystery_stack_triggers += other.mystery_stack_triggers;
        retriggers += other.retriggers;
        feature_spins += other.feature_spins;
        max_win = max(max_win, other.max_win);
        win_sq_sum += other.win_sq_sum;
        for (int i = 0; i < 4; i++) {
            reelset_usage_base[i] += other.reelset_usage_base[i];
            reelset_usage_normal[i] += other.reelset_usage_normal[i];
            reelset_usage_super[i] += other.reelset_usage_super[i];
        }
    }
};

struct Options {
    uint64_t spins = DEFAULT_SPINS;
    unsigned int seed = DEFAULT_SEED;
    int threads = 1;
    string output_path = "outputs/RTP_summary_new.md";
    string symbol_output_path = "outputs/symbol_distribution_new.csv";
    bool benchmark = false;
};

const char* symbolName(Symbol s) {
    switch (s) {
        case HV1: return "HV1";
        case HV2: return "HV2";
        case HV3: return "HV3";
        case HV4: return "HV4";
        case LV1: return "LV1";
        case LV2: return "LV2";
        case LV3: return "LV3";
        case LV4: return "LV4";
        case WILD: return "WILD";
        case HARD_HAT: return "HARD_HAT";
        case HAZARD_HARD_HAT: return "HAZARD_HARD_HAT";
        case HORIZONTAL_SAW_HARD_HAT: return "HORIZONTAL_SAW_HARD_HAT";
        case VERTICAL_SAW_HARD_HAT: return "VERTICAL_SAW_HARD_HAT";
        case SUPER_SAW_HARD_HAT: return "SUPER_SAW_HARD_HAT";
        case PIG1: return "PIG1";
        case PIG2: return "PIG2";
        case PIG3: return "PIG3";
        case TOOLBOX: return "TOOLBOX";
        case TAPE: return "TAPE";
        default: return "BLANK";
    }
}

bool isHardHat(Symbol s) {
    return s == HARD_HAT || s == HAZARD_HARD_HAT ||
           s == HORIZONTAL_SAW_HARD_HAT || s == VERTICAL_SAW_HARD_HAT ||
           s == SUPER_SAW_HARD_HAT;
}

bool isSawHat(Symbol s) {
    return s == HORIZONTAL_SAW_HARD_HAT ||
           s == VERTICAL_SAW_HARD_HAT ||
           s == SUPER_SAW_HARD_HAT;
}

int startRow(int active_rows) {
    return NO_OF_ROWS - active_rows;
}

int defaultThreadCount() {
    unsigned int hw = thread::hardware_concurrency();
    if (hw == 0) return 1;
    return max(1, static_cast<int>((hw + 11) / 12));
}

bool parseUint64(const string& text, uint64_t& out) {
    try {
        size_t used = 0;
        out = stoull(text, &used);
        return used == text.size();
    } catch (...) {
        return false;
    }
}

struct Engine {
    mt19937 rng;
    array<ReelSet, 12> reelsets{};

    explicit Engine(unsigned int seed) : rng(seed) {
        loadTables();
    }

    int getRandom(int a, int b) {
        uniform_int_distribution<int> dist(a, b);
        return dist(rng);
    }

    double getUniform() {
        uniform_real_distribution<double> dist(0.0, 1.0);
        return dist(rng);
    }

    int weightedPick(const vector<int>& weights) {
        int total = 0;
        for (int w : weights) {
            if (w > 0) total += w;
        }
        if (total <= 0) return -1;
        int roll = getRandom(1, total);
        int running = 0;
        for (int i = 0; i < static_cast<int>(weights.size()); i++) {
            if (weights[i] <= 0) continue;
            running += weights[i];
            if (roll <= running) return i;
        }
        return -1;
    }

    int pickWeightedInt(const vector<WeightedInt>& table) {
        vector<int> weights;
        weights.reserve(table.size());
        for (const auto& row : table) weights.push_back(row.weight);
        int idx = weightedPick(weights);
        return idx < 0 ? table.front().value : table[idx].value;
    }

    Symbol pickWeightedSymbol(const vector<WeightedSymbol>& table) {
        vector<int> weights;
        weights.reserve(table.size());
        for (const auto& row : table) weights.push_back(row.weight);
        int idx = weightedPick(weights);
        return idx < 0 ? table.front().value : table[idx].value;
    }

    double pickWeightedDouble(const vector<WeightedDouble>& table) {
        vector<int> weights;
        weights.reserve(table.size());
        for (const auto& row : table) weights.push_back(row.weight);
        int idx = weightedPick(weights);
        return idx < 0 ? table.front().value : table[idx].value;
    }

    array<Symbol, 100> buildPlaceholderStrip(int wild_count, int hh_count, int reel_index) {
        // Source: Excel sheet "Reelsets_12", rows 21-81.
        // The workbook provides placeholder strip counts, not final strips.
        array<Symbol, 100> strip{};
        const Symbol paying[] = {HV1, HV2, HV3, HV4, LV1, LV2, LV3, LV4};
        int idx = 0;
        for (int i = 0; i < wild_count && idx < 100; i++) strip[idx++] = WILD;
        for (int i = 0; i < hh_count && idx < 100; i++) strip[idx++] = HARD_HAT;
        int p = reel_index;
        while (idx < 100) {
            strip[idx++] = paying[p % 8];
            p++;
        }
        return strip;
    }

    void loadTables() {
        int id = 0;
        for (GameState state : {BASE_GAME, NORMAL_FREE_SPINS, SUPER_SAW_FREE_SPINS}) {
            for (int active_rows = 3; active_rows <= 6; active_rows++) {
                ReelSet rs;
                rs.state = state;
                rs.active_rows = active_rows;
                for (int reel = 0; reel < NO_OF_REELS; reel++) {
                    int wild_count = 0;
                    int hh_count = 0;
                    if (state == BASE_GAME) {
                        wild_count = reel == 0 ? 0 : 3;
                        hh_count = (reel == 0 || reel == 2 || reel == 4) ? 1 : 0;
                    } else if (state == NORMAL_FREE_SPINS) {
                        wild_count = reel == 0 ? 0 : 5;
                    } else {
                        wild_count = reel == 0 ? 0 : 6;
                    }
                    rs.reels[reel] = buildPlaceholderStrip(wild_count, hh_count, reel);
                }
                reelsets[id++] = rs;
            }
        }
    }

    const ReelSet& findReelSet(GameState state, int active_rows) const {
        int state_offset = state == BASE_GAME ? 0 : (state == NORMAL_FREE_SPINS ? 4 : 8);
        return reelsets[state_offset + (active_rows - 3)];
    }

    Window generateWindow(const ReelSet& reelset) {
        Window window{};
        for (int reel = 0; reel < NO_OF_REELS; reel++) {
            int stop = getRandom(0, 99);
            for (int row = 0; row < NO_OF_ROWS; row++) {
                window[row][reel] = reelset.reels[reel][(stop + row) % 100];
            }
        }
        return window;
    }

    int countHardHats(const Window& window, int active_rows) const {
        int count = 0;
        for (int row = startRow(active_rows); row < NO_OF_ROWS; row++) {
            for (int reel = 0; reel < NO_OF_REELS; reel++) {
                count += isHardHat(window[row][reel]) ? 1 : 0;
            }
        }
        return count;
    }

    vector<pair<int, int>> hardHatPositions(const Window& window, int active_rows) const {
        vector<pair<int, int>> positions;
        for (int row = startRow(active_rows); row < NO_OF_ROWS; row++) {
            for (int reel = 0; reel < NO_OF_REELS; reel++) {
                if (isHardHat(window[row][reel])) positions.push_back({row, reel});
            }
        }
        return positions;
    }

    bool hasSuperSawHat(const Window& window, int active_rows) const {
        for (int row = startRow(active_rows); row < NO_OF_ROWS; row++) {
            for (int reel = 0; reel < NO_OF_REELS; reel++) {
                if (window[row][reel] == SUPER_SAW_HARD_HAT) return true;
            }
        }
        return false;
    }

    Symbol mysteryStackSymbol() {
        // Source: Excel sheet "Base_Tables", Table K.
        return pickWeightedSymbol({{PIG1, 2500}, {PIG2, 2500}, {PIG3, 2000}, {TOOLBOX, 1800}, {TAPE, 1200}});
    }

    void applyMysteryStack(Window& window, int active_rows, Statistics& stats) {
        // Source: Excel sheet "Base_Tables", Tables I-L.
        if (pickWeightedInt({{0, 8800}, {1, 1200}}) == 0) return;
        stats.mystery_stack_triggers++;
        int reels_affected = pickWeightedInt({{1, 6000}, {2, 3000}, {3, 900}, {4, 100}});
        int positions_per_reel = pickWeightedInt({{2, 5000}, {3, 3000}, {4, 1500}, {active_rows, 500}});
        Symbol stack_symbol = mysteryStackSymbol();
        vector<int> reels = {0, 1, 2, 3, 4};
        shuffle(reels.begin(), reels.end(), rng);
        reels.resize(reels_affected);
        for (int reel : reels) {
            vector<int> rows;
            for (int row = startRow(active_rows); row < NO_OF_ROWS; row++) rows.push_back(row);
            shuffle(rows.begin(), rows.end(), rng);
            rows.resize(min(positions_per_reel, active_rows));
            for (int row : rows) window[row][reel] = stack_symbol;
        }
    }

    Symbol baseHardHatType(int active_rows) {
        // Source: Excel sheet "Base_Tables", Table C.
        if (active_rows == 3) return pickWeightedSymbol({{HARD_HAT, 7900}, {HAZARD_HARD_HAT, 1500}, {HORIZONTAL_SAW_HARD_HAT, 250}, {VERTICAL_SAW_HARD_HAT, 250}, {SUPER_SAW_HARD_HAT, 100}});
        if (active_rows == 4) return pickWeightedSymbol({{HARD_HAT, 7600}, {HAZARD_HARD_HAT, 1300}, {HORIZONTAL_SAW_HARD_HAT, 450}, {VERTICAL_SAW_HARD_HAT, 450}, {SUPER_SAW_HARD_HAT, 200}});
        if (active_rows == 5) return pickWeightedSymbol({{HARD_HAT, 7300}, {HAZARD_HARD_HAT, 900}, {HORIZONTAL_SAW_HARD_HAT, 650}, {VERTICAL_SAW_HARD_HAT, 650}, {SUPER_SAW_HARD_HAT, 500}});
        return pickWeightedSymbol({{HARD_HAT, 7000}, {HAZARD_HARD_HAT, 0}, {HORIZONTAL_SAW_HARD_HAT, 850}, {VERTICAL_SAW_HARD_HAT, 850}, {SUPER_SAW_HARD_HAT, 1300}});
    }

    Symbol featureHardHatType(GameState state) {
        // Source: Excel sheets "Normal_FS_Tables" FS-C and "SuperSaw_FS_Tables" SS-C.
        if (state == NORMAL_FREE_SPINS) {
            return pickWeightedSymbol({{HARD_HAT, 6800}, {HAZARD_HARD_HAT, 1000}, {HORIZONTAL_SAW_HARD_HAT, 1100}, {VERTICAL_SAW_HARD_HAT, 1100}});
        }
        return pickWeightedSymbol({{HARD_HAT, 4200}, {HAZARD_HARD_HAT, 700}, {HORIZONTAL_SAW_HARD_HAT, 2500}, {VERTICAL_SAW_HARD_HAT, 2500}, {SUPER_SAW_HARD_HAT, 100}});
    }

    int weightedReel() {
        // Source: Excel sheet "Base_Tables", Table G.
        return pickWeightedInt({{0, 2200}, {1, 1600}, {2, 2600}, {3, 1600}, {4, 2000}});
    }

    int weightedRow(int active_rows) {
        // Source: Excel sheet "Base_Tables", Table H.
        if (active_rows == 3) return pickWeightedInt({{3, 3000}, {4, 4000}, {5, 3000}});
        if (active_rows == 4) return pickWeightedInt({{2, 2400}, {3, 2800}, {4, 2400}, {5, 2400}});
        if (active_rows == 5) return pickWeightedInt({{1, 1700}, {2, 2100}, {3, 2200}, {4, 2000}, {5, 2000}});
        return pickWeightedInt({{0, 1300}, {1, 1500}, {2, 1700}, {3, 1800}, {4, 1900}, {5, 1800}});
    }

    void placeHardHats(Window& window, GameState state, int active_rows) {
        bool trigger = false;
        int count = 0;
        if (state == BASE_GAME) {
            trigger = pickWeightedInt({{0, 7200}, {1, 2800}}) == 1;
            if (trigger) count = pickWeightedInt({{1, 4200}, {2, 2800}, {3, 1700}, {4, 900}, {5, 350}, {6, 50}});
        } else if (state == NORMAL_FREE_SPINS) {
            trigger = pickWeightedInt({{0, 4500}, {1, 5500}}) == 1;
            if (trigger) count = pickWeightedInt({{1, 4000}, {2, 3000}, {3, 1700}, {4, 900}, {5, 300}, {6, 100}});
        } else {
            trigger = pickWeightedInt({{0, 2500}, {1, 7500}}) == 1;
            if (trigger) count = pickWeightedInt({{1, 2500}, {2, 3000}, {3, 2300}, {4, 1400}, {5, 600}, {6, 200}});
        }
        if (!trigger) return;

        for (int i = 0; i < count; i++) {
            for (int attempt = 0; attempt < 20; attempt++) {
                int row = weightedRow(active_rows);
                int reel = weightedReel();
                if (row < startRow(active_rows) || isHardHat(window[row][reel])) continue;
                window[row][reel] = state == BASE_GAME ? baseHardHatType(active_rows) : featureHardHatType(state);
                break;
            }
        }
    }

    int resolveHazardExpansion(const Window& window, int active_rows) {
        bool has_hazard = false;
        for (int row = startRow(active_rows); row < NO_OF_ROWS; row++) {
            for (int reel = 0; reel < NO_OF_REELS; reel++) {
                if (window[row][reel] == HAZARD_HARD_HAT) has_hazard = true;
            }
        }
        if (!has_hazard || active_rows >= 6) return active_rows;
        // Source: Excel sheet "Base_Tables", Table D.
        // Assumption carried from core_new.cpp: free-spin hazard uses the same table.
        int unlock = pickWeightedInt({{1, 7800}, {2, 1900}, {3, 300}});
        return min(6, active_rows + unlock);
    }

    bool rollGirderTrigger(int hard_hat_count) {
        // Source: Excel sheet "Base_Tables", Table E.
        if (hard_hat_count <= 0 || hard_hat_count >= 6) return false;
        double p = getUniform();
        if (hard_hat_count == 1) return p < 0.0005;
        if (hard_hat_count == 2) return p < 0.0015;
        if (hard_hat_count == 3) return p < 0.0075;
        if (hard_hat_count == 4) return p < 0.03;
        return p < 0.18;
    }

    Symbol girderCompletionSymbol() {
        // Source: Excel sheet "Base_Tables", Table F.
        return pickWeightedSymbol({{HARD_HAT, 8500}, {HAZARD_HARD_HAT, 700}, {HORIZONTAL_SAW_HARD_HAT, 300}, {VERTICAL_SAW_HARD_HAT, 300}, {SUPER_SAW_HARD_HAT, 200}});
    }

    void applyGirder(Window& window, int active_rows, Statistics& stats) {
        int hard_hat_count = countHardHats(window, active_rows);
        if (!rollGirderTrigger(hard_hat_count)) return;
        stats.girder_triggers++;
        int missing = max(0, 6 - hard_hat_count);
        vector<int> positions;
        for (int row = startRow(active_rows); row < NO_OF_ROWS; row++) {
            for (int reel = 0; reel < NO_OF_REELS; reel++) {
                if (!isHardHat(window[row][reel])) positions.push_back(row * NO_OF_REELS + reel);
            }
        }
        shuffle(positions.begin(), positions.end(), rng);
        for (int i = 0; i < missing && i < static_cast<int>(positions.size()); i++) {
            int pos = positions[i];
            window[pos / NO_OF_REELS][pos % NO_OF_REELS] = girderCompletionSymbol();
        }
    }

    long long evaluateWaysWins(const Window&, int) {
        // TODO(Math model): add real line/ways paytable when supplied.
        return 0;
    }

    long long awardOverflowPrize() {
        // Source: Excel sheet "Normal_FS_Tables", Table FS-D.
        return static_cast<long long>(pickWeightedDouble({{5, 4500}, {10, 3000}, {15, 1600}, {25, 700}, {50, 180}, {100, 20}}) * BET + 0.5);
    }

    long long awardHousePrize(Frame frame, long long& jackpot_win) {
        if (frame == STRAW_FRAME) {
            return static_cast<long long>(pickWeightedDouble({{0.5, 5000}, {0.75, 3000}, {1.0, 2000}}) * BET + 0.5);
        }
        if (frame == STICK_FRAME) {
            return static_cast<long long>(pickWeightedDouble({{1.5, 3500}, {2.0, 2500}, {3.0, 2000}, {4.5, 1500}, {7.5, 500}}) * BET + 0.5);
        }
        if (frame == BRICK_FRAME) {
            vector<int> weights = {3500, 2500, 1600, 1000, 700, 400, 180, 90, 25, 5};
            int idx = weightedPick(weights);
            const double values[] = {7.5, 10, 15, 25, 50, 100, 25, 100, 500, 5000};
            long long win = static_cast<long long>(values[idx] * BET + 0.5);
            if (idx >= 6) jackpot_win += win;
            return win;
        }
        return 0;
    }

    vector<pair<int, int>> positionsWithFrame(const FrameWindow& frames, int active_rows, Frame frame) const {
        vector<pair<int, int>> positions;
        for (int row = startRow(active_rows); row < NO_OF_ROWS; row++) {
            for (int reel = 0; reel < NO_OF_REELS; reel++) {
                if (frames[row][reel] == frame) positions.push_back({row, reel});
            }
        }
        return positions;
    }

    long long applyFrameHit(FrameWindow& frames, int row, int reel, int active_rows) {
        if (frames[row][reel] == NO_FRAME) {
            frames[row][reel] = STRAW_FRAME;
            return 0;
        }
        if (frames[row][reel] == STRAW_FRAME) {
            frames[row][reel] = STICK_FRAME;
            return 0;
        }
        if (frames[row][reel] == STICK_FRAME) {
            frames[row][reel] = BRICK_FRAME;
            return 0;
        }
        auto sticks = positionsWithFrame(frames, active_rows, STICK_FRAME);
        if (!sticks.empty()) {
            auto pos = sticks[getRandom(0, static_cast<int>(sticks.size()) - 1)];
            frames[pos.first][pos.second] = BRICK_FRAME;
            return 0;
        }
        auto straws = positionsWithFrame(frames, active_rows, STRAW_FRAME);
        if (!straws.empty()) {
            auto pos = straws[getRandom(0, static_cast<int>(straws.size()) - 1)];
            frames[pos.first][pos.second] = STICK_FRAME;
            return 0;
        }
        auto empty = positionsWithFrame(frames, active_rows, NO_FRAME);
        if (!empty.empty()) {
            auto pos = empty[getRandom(0, static_cast<int>(empty.size()) - 1)];
            frames[pos.first][pos.second] = STRAW_FRAME;
            return 0;
        }
        return awardOverflowPrize();
    }

    long long applySawPath(FrameWindow& frames, int row, int reel, int active_rows, Symbol saw_symbol) {
        long long win = 0;
        array<int, NO_OF_ROWS * NO_OF_REELS> seen{};
        auto hit = [&](int r, int c) {
            int idx = r * NO_OF_REELS + c;
            if (seen[idx]) return;
            seen[idx] = 1;
            win += applyFrameHit(frames, r, c, active_rows);
        };
        if (saw_symbol == HORIZONTAL_SAW_HARD_HAT || saw_symbol == SUPER_SAW_HARD_HAT) {
            for (int c = 0; c < NO_OF_REELS; c++) if (c != reel) hit(row, c);
        }
        if (saw_symbol == VERTICAL_SAW_HARD_HAT || saw_symbol == SUPER_SAW_HARD_HAT) {
            for (int r = startRow(active_rows); r < NO_OF_ROWS; r++) if (r != row) hit(r, reel);
        }
        return win;
    }

    FeatureResult playFreeGames(GameState feature_state, int active_rows, const vector<pair<int, int>>& trigger_positions) {
        FeatureResult result;
        FrameWindow frames{};
        for (auto& row : frames) row.fill(NO_FRAME);
        for (const auto& pos : trigger_positions) frames[pos.first][pos.second] = STRAW_FRAME;

        int remaining_spins = 6;
        while (remaining_spins > 0) {
            result.spins_played++;
            Window window = generateWindow(findReelSet(feature_state, active_rows));
            placeHardHats(window, feature_state, active_rows);
            active_rows = resolveHazardExpansion(window, active_rows);
            auto hats = hardHatPositions(window, active_rows);
            int retrigger_symbols = 0;
            for (const auto& pos : hats) {
                Symbol s = window[pos.first][pos.second];
                result.overflow_win += applyFrameHit(frames, pos.first, pos.second, active_rows);
                if (feature_state == NORMAL_FREE_SPINS || isSawHat(s) || s == HAZARD_HARD_HAT) retrigger_symbols++;
            }
            for (const auto& pos : hats) {
                Symbol s = window[pos.first][pos.second];
                if (isSawHat(s)) result.overflow_win += applySawPath(frames, pos.first, pos.second, active_rows, s);
            }
            if (retrigger_symbols >= 3) {
                remaining_spins++;
                result.retriggers++;
            }
            remaining_spins--;
        }

        for (int row = 0; row < NO_OF_ROWS; row++) {
            for (int reel = 0; reel < NO_OF_REELS; reel++) {
                if (frames[row][reel] != NO_FRAME) result.total_win += awardHousePrize(frames[row][reel], result.jackpot_win);
            }
        }
        result.total_win += result.overflow_win;
        return result;
    }

    SpinResult spinBaseGame(Statistics& stats) {
        SpinResult result;
        int active_rows = 3;
        Window window = generateWindow(findReelSet(BASE_GAME, active_rows));
        stats.reelset_usage_base[active_rows - 3]++;
        applyMysteryStack(window, active_rows, stats);
        placeHardHats(window, BASE_GAME, active_rows);
        active_rows = resolveHazardExpansion(window, active_rows);
        result.base_win = evaluateWaysWins(window, active_rows);
        applyGirder(window, active_rows, stats);
        active_rows = resolveHazardExpansion(window, active_rows);
        result.active_rows = active_rows;
        result.window = window;
        result.hard_hat_count = countHardHats(window, active_rows);

        if (result.hard_hat_count >= 6) {
            bool super_saw = hasSuperSawHat(window, active_rows);
            FeatureResult feature = playFreeGames(super_saw ? SUPER_SAW_FREE_SPINS : NORMAL_FREE_SPINS,
                                                 active_rows,
                                                 hardHatPositions(window, active_rows));
            long long capped_total = min(MAX_WIN_CREDITS, result.base_win + feature.total_win);
            result.feature_win = max(0LL, capped_total - result.base_win);
            result.overflow_win = feature.overflow_win;
            result.jackpot_win = feature.jackpot_win;
            stats.feature_spins += feature.spins_played;
            stats.retriggers += feature.retriggers;
            if (super_saw) {
                result.triggered_super_fs = true;
                stats.super_saw_fs_triggers++;
                stats.super_saw_fs_win += result.feature_win;
                stats.reelset_usage_super[active_rows - 3]++;
            } else {
                result.triggered_normal_fs = true;
                stats.normal_fs_triggers++;
                stats.normal_fs_win += result.feature_win;
                stats.reelset_usage_normal[active_rows - 3]++;
            }
        }
        return result;
    }
};

void updateStatistics(Statistics& stats, const SpinResult& result) {
    long long total = result.base_win + result.feature_win;
    stats.spins++;
    stats.total_bet += BET;
    stats.total_win += total;
    stats.base_ways_win += result.base_win;
    stats.overflow_win += result.overflow_win;
    stats.jackpot_win += result.jackpot_win;
    stats.max_win = max(stats.max_win, total);
    stats.win_sq_sum += static_cast<long double>(total) * static_cast<long double>(total);
    if (total > 0) stats.hits++;
    if (result.base_win > 0) stats.base_hits++;
}

Statistics runSingleThread(uint64_t spins, unsigned int seed) {
    Engine engine(seed);
    Statistics stats;
    for (uint64_t i = 0; i < spins; i++) {
        updateStatistics(stats, engine.spinBaseGame(stats));
    }
    return stats;
}

Statistics runSimulation(uint64_t spins, unsigned int seed, int requested_threads) {
    int threads = max(1, requested_threads);
    if (threads == 1 || spins < static_cast<uint64_t>(threads)) {
        return runSingleThread(spins, seed);
    }

    vector<thread> workers;
    vector<Statistics> partials(threads);
    uint64_t base = spins / threads;
    uint64_t extra = spins % threads;
    for (int t = 0; t < threads; t++) {
        uint64_t count = base + (t < static_cast<int>(extra) ? 1 : 0);
        unsigned int thread_seed = seed + static_cast<unsigned int>(t * 1000003U);
        workers.emplace_back([count, thread_seed, &partials, t]() {
            partials[t] = runSingleThread(count, thread_seed);
        });
    }
    for (auto& worker : workers) worker.join();

    Statistics stats;
    for (const auto& part : partials) stats.merge(part);
    return stats;
}

double rtp(long long win, const Statistics& stats) {
    return stats.total_bet > 0 ? static_cast<double>(win) / static_cast<double>(stats.total_bet) : 0.0;
}

double frequency(uint64_t events, uint64_t trials) {
    return events > 0 && trials > 0 ? static_cast<double>(trials) / static_cast<double>(events) : 0.0;
}

string pct(double value) {
    ostringstream out;
    out << fixed << setprecision(4) << value * 100.0 << "%";
    return out.str();
}

void printWindow(ostream& out, const Window& window) {
    for (int row = 0; row < NO_OF_ROWS; row++) {
        for (int reel = 0; reel < NO_OF_REELS; reel++) {
            out << left << setw(24) << symbolName(window[row][reel]);
        }
        out << "\n";
    }
}

void writeRtpSummary(ostream& out, const Statistics& stats, unsigned int seed, int threads, double elapsed_seconds) {
    long double mean = stats.spins > 0 ? static_cast<long double>(stats.total_win) / stats.spins : 0.0L;
    long double second = stats.spins > 0 ? stats.win_sq_sum / stats.spins : 0.0L;
    double stddev = sqrt(static_cast<double>(max(0.0L, second - mean * mean))) / BET;
    uint64_t feature_triggers = stats.normal_fs_triggers + stats.super_saw_fs_triggers;
    double avg_feature = feature_triggers > 0
        ? static_cast<double>(stats.normal_fs_win + stats.super_saw_fs_win) / feature_triggers / BET
        : 0.0;

    out << "# RTP Summary New\n\n";
    out << "- Spins: " << stats.spins << "\n";
    out << "- Seed: " << seed << "\n";
    out << "- Threads: " << threads << "\n";
    out << "- Elapsed seconds: " << fixed << setprecision(6) << elapsed_seconds << "\n\n";
    out << "| Component | Credits | RTP |\n";
    out << "|---|---:|---:|\n";
    out << "| Total bet | " << stats.total_bet << " | |\n";
    out << "| Total win | " << stats.total_win << " | " << pct(rtp(stats.total_win, stats)) << " |\n";
    out << "| Base ways wins | " << stats.base_ways_win << " | " << pct(rtp(stats.base_ways_win, stats)) << " |\n";
    out << "| Normal free spins | " << stats.normal_fs_win << " | " << pct(rtp(stats.normal_fs_win, stats)) << " |\n";
    out << "| Super Saw free spins | " << stats.super_saw_fs_win << " | " << pct(rtp(stats.super_saw_fs_win, stats)) << " |\n";
    out << "| Overflow prizes | " << stats.overflow_win << " | " << pct(rtp(stats.overflow_win, stats)) << " |\n";
    out << "| Jackpot prizes | " << stats.jackpot_win << " | " << pct(rtp(stats.jackpot_win, stats)) << " |\n\n";
    out << "| Metric | Count | Frequency |\n";
    out << "|---|---:|---:|\n";
    out << "| Any hit | " << stats.hits << " | 1 in " << frequency(stats.hits, stats.spins) << " |\n";
    out << "| Base hit | " << stats.base_hits << " | 1 in " << frequency(stats.base_hits, stats.spins) << " |\n";
    out << "| Normal FS trigger | " << stats.normal_fs_triggers << " | 1 in " << frequency(stats.normal_fs_triggers, stats.spins) << " |\n";
    out << "| Super Saw FS trigger | " << stats.super_saw_fs_triggers << " | 1 in " << frequency(stats.super_saw_fs_triggers, stats.spins) << " |\n";
    out << "| Any feature trigger | " << feature_triggers << " | 1 in " << frequency(feature_triggers, stats.spins) << " |\n";
    out << "| Girder trigger | " << stats.girder_triggers << " | 1 in " << frequency(stats.girder_triggers, stats.spins) << " |\n";
    out << "| Mystery Stack trigger | " << stats.mystery_stack_triggers << " | 1 in " << frequency(stats.mystery_stack_triggers, stats.spins) << " |\n\n";
    out << "- Average feature win: " << fixed << setprecision(4) << avg_feature << "x bet\n";
    out << "- Maximum win observed: " << static_cast<double>(stats.max_win) / BET << "x bet\n";
    out << "- Standard deviation estimate: " << stddev << "x bet\n";
    out << "- TODO: final strips and paytable are still missing from the source workbook.\n";
}

void writeSymbolDistribution(ostream& out, const Statistics& stats) {
    out << "Symbol,Occurrence,Win,Mode,Hits,RTP\n";
    out << "Base Ways,0," << stats.base_ways_win << ",Base,0," << rtp(stats.base_ways_win, stats) << "\n";
    out << "Normal Free Spins," << stats.normal_fs_triggers << "," << stats.normal_fs_win << ",Feature," << stats.normal_fs_triggers << "," << rtp(stats.normal_fs_win, stats) << "\n";
    out << "Super Saw Free Spins," << stats.super_saw_fs_triggers << "," << stats.super_saw_fs_win << ",Feature," << stats.super_saw_fs_triggers << "," << rtp(stats.super_saw_fs_win, stats) << "\n";
    out << "Overflow Prizes,0," << stats.overflow_win << ",Feature,0," << rtp(stats.overflow_win, stats) << "\n";
    out << "Jackpot Prizes,0," << stats.jackpot_win << ",Feature,0," << rtp(stats.jackpot_win, stats) << "\n";
}

void ensureParentDirectory(const string& path) {
    filesystem::path p(path);
    if (p.has_parent_path()) filesystem::create_directories(p.parent_path());
}

void printUsage(ostream& out) {
    out << "Usage: ./efficient_core_new [--spins N] [--seed N] [--threads N|--single-thread] [--benchmark]\n"
        << "                           [--output outputs/RTP_summary_new.md]\n"
        << "                           [--symbol-output outputs/symbol_distribution_new.csv]\n";
}

bool parseOptions(int argc, char** argv, Options& options) {
    options.threads = defaultThreadCount();
    for (int i = 1; i < argc; i++) {
        string arg = argv[i];
        if (arg == "--spins" && i + 1 < argc) {
            uint64_t value = 0;
            if (!parseUint64(argv[++i], value)) return false;
            options.spins = value;
        } else if (arg == "--seed" && i + 1 < argc) {
            uint64_t value = 0;
            if (!parseUint64(argv[++i], value)) return false;
            options.seed = static_cast<unsigned int>(value);
        } else if (arg == "--threads" && i + 1 < argc) {
            uint64_t value = 0;
            if (!parseUint64(argv[++i], value) || value == 0) return false;
            options.threads = static_cast<int>(min<uint64_t>(value, 1024));
        } else if (arg == "--single-thread") {
            options.threads = 1;
        } else if (arg == "--benchmark") {
            options.benchmark = true;
            options.spins = DEV_BENCH_SPINS;
        } else if (arg == "--output" && i + 1 < argc) {
            options.output_path = argv[++i];
        } else if (arg == "--symbol-output" && i + 1 < argc) {
            options.symbol_output_path = argv[++i];
        } else if (arg == "--help") {
            printUsage(cout);
            exit(0);
        } else {
            return false;
        }
    }
    return true;
}

} // namespace hnp_new

#ifndef EFFICIENT_CORE_NEW_LIBRARY
int main(int argc, char** argv) {
    hnp_new::Options options;
    if (!hnp_new::parseOptions(argc, argv, options)) {
        hnp_new::printUsage(cerr);
        return 1;
    }

    auto start = chrono::steady_clock::now();
    hnp_new::Statistics stats = hnp_new::runSimulation(options.spins, options.seed, options.threads);
    auto end = chrono::steady_clock::now();
    double elapsed = chrono::duration<double>(end - start).count();

    hnp_new::writeRtpSummary(cout, stats, options.seed, options.threads, elapsed);

    hnp_new::ensureParentDirectory(options.output_path);
    ofstream summary(options.output_path);
    hnp_new::writeRtpSummary(summary, stats, options.seed, options.threads, elapsed);

    hnp_new::ensureParentDirectory(options.symbol_output_path);
    ofstream symbol_out(options.symbol_output_path);
    hnp_new::writeSymbolDistribution(symbol_out, stats);

    return 0;
}
#endif
