#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <random>
#include <sstream>
#include <string>
#include <vector>

using namespace std;

// New simulator built from:
// - Game_Rules/*.json
// - Huff_N_Puff_Highrise_Math_Model_12_Reelsets.xlsx
// Reference core/core.cpp was used only for structure/style.
//
// Important source-data gaps:
// TODO(Math model): The workbook does not provide final reel strips or a
// symbol paytable. Sheet "Reelsets_12" contains placeholder strip build counts
// and says to replace them after a simulation pass. Because Task 4 forbids
// inventing paytables, base ways evaluation is tracked but pays 0 until a real
// paytable is supplied.

enum Symbol {
    HV1, HV2, HV3, HV4, LV1, LV2, LV3, LV4,
    WILD,
    HARD_HAT,
    HAZARD_HARD_HAT,
    HORIZONTAL_SAW_HARD_HAT,
    VERTICAL_SAW_HARD_HAT,
    SUPER_SAW_HARD_HAT,
    PIG1, PIG2, PIG3, TOOLBOX, TAPE,
    BLANK
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

const int no_of_reels = 5;
const int no_of_rows = 6;
const int bet = 20;
const int max_win_xbet = 10000;
const long long max_win_credits = max_win_xbet * bet;

using Window = array<array<Symbol, no_of_reels>, no_of_rows>;
using FrameWindow = array<array<Frame, no_of_reels>, no_of_rows>;

std::random_device rd;
std::mt19937 rng(
    std::chrono::high_resolution_clock::now()
        .time_since_epoch()
        .count()
);

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
    string id;
    GameState state;
    int active_rows;
    array<vector<Symbol>, no_of_reels> reels;
};

struct SpinResult {
    long long base_win = 0;
    long long feature_win = 0;
    long long overflow_win = 0;
    long long jackpot_win = 0;
    int active_rows = 3;
    bool triggered_normal_fs = false;
    bool triggered_super_fs = false;
    bool hit = false;
};

struct FeatureResult {
    long long total_win = 0;
    long long overflow_win = 0;
    long long jackpot_win = 0;
    int spins_played = 0;
    int retriggers = 0;
    int house_prizes = 0;
};

struct Statistics {
    long long spins = 0;
    long long total_bet = 0;
    long long total_win = 0;
    long long base_ways_win = 0;
    long long normal_fs_win = 0;
    long long super_saw_fs_win = 0;
    long long overflow_win = 0;
    long long jackpot_win = 0;
    long long hits = 0;
    long long base_hits = 0;
    long long normal_fs_triggers = 0;
    long long super_saw_fs_triggers = 0;
    long long girder_triggers = 0;
    long long mystery_stack_triggers = 0;
    long long retriggers = 0;
    long long feature_spins = 0;
    long long max_win = 0;
    double win_sq_sum = 0.0;
    array<long long, 4> reelset_usage_base = {};
    array<long long, 4> reelset_usage_normal = {};
    array<long long, 4> reelset_usage_super = {};
};

struct GameConfig {
    int default_spins = 100000;
    unsigned int seed = 0;
    string output_path = "outputs/RTP_summary.md";
};

string symbolName(Symbol s) {
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
    return s == HARD_HAT ||
           s == HAZARD_HARD_HAT ||
           s == HORIZONTAL_SAW_HARD_HAT ||
           s == VERTICAL_SAW_HARD_HAT ||
           s == SUPER_SAW_HARD_HAT;
}

bool isSawHat(Symbol s) {
    return s == HORIZONTAL_SAW_HARD_HAT ||
           s == VERTICAL_SAW_HARD_HAT ||
           s == SUPER_SAW_HARD_HAT;
}

int startRow(int active_rows) {
    return no_of_rows - active_rows;
}

int getRandom(int a, int b) {
    std::uniform_int_distribution<int> dist(a, b);
    return dist(rng);
}

double getUniform() {
    std::uniform_real_distribution<double> dist(0.0, 1.0);
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
    for (const auto& row : table) weights.push_back(row.weight);
    int idx = weightedPick(weights);
    return idx < 0 ? table.front().value : table[idx].value;
}

double pickWeightedDouble(const vector<WeightedDouble>& table) {
    vector<int> weights;
    for (const auto& row : table) weights.push_back(row.weight);
    int idx = weightedPick(weights);
    return idx < 0 ? table.front().value : table[idx].value;
}

Symbol pickWeightedSymbol(const vector<WeightedSymbol>& table) {
    vector<int> weights;
    for (const auto& row : table) weights.push_back(row.weight);
    int idx = weightedPick(weights);
    return idx < 0 ? table.front().value : table[idx].value;
}

vector<int> randomPositionsWithoutReplacement(vector<int> positions, int count) {
    shuffle(positions.begin(), positions.end(), rng);
    if (count < static_cast<int>(positions.size())) {
        positions.resize(count);
    }
    return positions;
}

// Source: Excel sheet "Reelsets_12", rows 21-81.
// Represents the available state/active-row reelsets and placeholder strip
// build skeleton. Actual symbol strips are absent in the workbook, so this
// constructs deterministic placeholder strips from the supplied length, wild
// count, and optional normal-HH count. Paying-symbol composition is balanced
// and must be replaced when final strips are supplied.
vector<Symbol> buildPlaceholderStrip(int length, int wild_count, int hh_count, int reel_index) {
    vector<Symbol> strip;
    const Symbol paying[] = {HV1, HV2, HV3, HV4, LV1, LV2, LV3, LV4};

    for (int i = 0; i < wild_count; i++) strip.push_back(WILD);
    for (int i = 0; i < hh_count; i++) strip.push_back(HARD_HAT);

    int idx = reel_index;
    while (static_cast<int>(strip.size()) < length) {
        strip.push_back(paying[idx % 8]);
        idx++;
    }
    return strip;
}

vector<ReelSet> loadTables() {
    vector<ReelSet> reelsets;
    int id = 1;
    for (GameState state : {BASE_GAME, NORMAL_FREE_SPINS, SUPER_SAW_FREE_SPINS}) {
        for (int active_rows = 3; active_rows <= 6; active_rows++) {
            ReelSet rs;
            rs.id = "R" + to_string(id++);
            rs.state = state;
            rs.active_rows = active_rows;
            for (int reel = 0; reel < no_of_reels; reel++) {
                int wild_count = 0;
                int hh_count = 0;
                if (state == BASE_GAME) {
                    wild_count = reel == 0 ? 0 : 3;
                    hh_count = (reel == 0 || reel == 2 || reel == 4) ? 1 : 0;
                } else if (state == NORMAL_FREE_SPINS) {
                    wild_count = reel == 0 ? 0 : 5;
                    hh_count = 0;
                } else {
                    wild_count = reel == 0 ? 0 : 6;
                    hh_count = 0;
                }
                rs.reels[reel] = buildPlaceholderStrip(100, wild_count, hh_count, reel);
            }
            reelsets.push_back(rs);
        }
    }
    return reelsets;
}

const ReelSet& findReelSet(const vector<ReelSet>& reelsets, GameState state, int active_rows) {
    for (const auto& rs : reelsets) {
        if (rs.state == state && rs.active_rows == active_rows) return rs;
    }
    return reelsets.front();
}

Window generateWindow(const ReelSet& reelset) {
    Window window{};
    for (int reel = 0; reel < no_of_reels; reel++) {
        int stop = getRandom(0, static_cast<int>(reelset.reels[reel].size()) - 1);
        for (int row = 0; row < no_of_rows; row++) {
            int idx = (stop + row) % static_cast<int>(reelset.reels[reel].size());
            window[row][reel] = reelset.reels[reel][idx];
        }
    }
    return window;
}

vector<pair<int, int>> activePositions(int active_rows) {
    vector<pair<int, int>> positions;
    for (int row = startRow(active_rows); row < no_of_rows; row++) {
        for (int reel = 0; reel < no_of_reels; reel++) {
            positions.push_back({row, reel});
        }
    }
    return positions;
}

int countHardHats(const Window& window, int active_rows, bool include_super = true) {
    int count = 0;
    for (int row = startRow(active_rows); row < no_of_rows; row++) {
        for (int reel = 0; reel < no_of_reels; reel++) {
            if (!include_super && window[row][reel] == SUPER_SAW_HARD_HAT) continue;
            if (isHardHat(window[row][reel])) count++;
        }
    }
    return count;
}

vector<pair<int, int>> hardHatPositions(const Window& window, int active_rows) {
    vector<pair<int, int>> positions;
    for (int row = startRow(active_rows); row < no_of_rows; row++) {
        for (int reel = 0; reel < no_of_reels; reel++) {
            if (isHardHat(window[row][reel])) positions.push_back({row, reel});
        }
    }
    return positions;
}

bool hasSuperSawHat(const Window& window, int active_rows) {
    for (int row = startRow(active_rows); row < no_of_rows; row++) {
        for (int reel = 0; reel < no_of_reels; reel++) {
            if (window[row][reel] == SUPER_SAW_HARD_HAT) return true;
        }
    }
    return false;
}

// Source: Excel sheet "Base_Tables", Table K.
Symbol mysteryStackSymbol() {
    return pickWeightedSymbol({
        {PIG1, 2500},
        {PIG2, 2500},
        {PIG3, 2000},
        {TOOLBOX, 1800},
        {TAPE, 1200},
    });
}

// Source: Excel sheet "Base_Tables", Tables I-L.
void applyMysteryStack(Window& window, int active_rows, Statistics& stats) {
    bool trigger = pickWeightedInt({{0, 8800}, {1, 1200}}) == 1;
    if (!trigger) return;

    stats.mystery_stack_triggers++;
    int reels_affected = pickWeightedInt({{1, 6000}, {2, 3000}, {3, 900}, {4, 100}});
    int positions_per_reel = pickWeightedInt({{2, 5000}, {3, 3000}, {4, 1500}, {active_rows, 500}});
    Symbol stack_symbol = mysteryStackSymbol();

    vector<int> reel_ids = {0, 1, 2, 3, 4};
    shuffle(reel_ids.begin(), reel_ids.end(), rng);
    reel_ids.resize(reels_affected);

    for (int reel : reel_ids) {
        vector<int> rows;
        for (int row = startRow(active_rows); row < no_of_rows; row++) rows.push_back(row);
        rows = randomPositionsWithoutReplacement(rows, min(positions_per_reel, active_rows));
        for (int row : rows) {
            window[row][reel] = stack_symbol;
        }
    }
}

// Source: Excel sheet "Base_Tables", Table C.
Symbol baseHardHatType(int active_rows) {
    if (active_rows == 3) {
        return pickWeightedSymbol({{HARD_HAT, 7900}, {HAZARD_HARD_HAT, 1500}, {HORIZONTAL_SAW_HARD_HAT, 250}, {VERTICAL_SAW_HARD_HAT, 250}, {SUPER_SAW_HARD_HAT, 100}});
    }
    if (active_rows == 4) {
        return pickWeightedSymbol({{HARD_HAT, 7600}, {HAZARD_HARD_HAT, 1300}, {HORIZONTAL_SAW_HARD_HAT, 450}, {VERTICAL_SAW_HARD_HAT, 450}, {SUPER_SAW_HARD_HAT, 200}});
    }
    if (active_rows == 5) {
        return pickWeightedSymbol({{HARD_HAT, 7300}, {HAZARD_HARD_HAT, 900}, {HORIZONTAL_SAW_HARD_HAT, 650}, {VERTICAL_SAW_HARD_HAT, 650}, {SUPER_SAW_HARD_HAT, 500}});
    }
    return pickWeightedSymbol({{HARD_HAT, 7000}, {HAZARD_HARD_HAT, 0}, {HORIZONTAL_SAW_HARD_HAT, 850}, {VERTICAL_SAW_HARD_HAT, 850}, {SUPER_SAW_HARD_HAT, 1300}});
}

// Source: Excel sheet "Normal_FS_Tables", Table FS-C, and
// "SuperSaw_FS_Tables", Table SS-C.
Symbol featureHardHatType(GameState state) {
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
        // Source: Excel sheet "Base_Tables", Tables A-B.
        trigger = pickWeightedInt({{0, 7200}, {1, 2800}}) == 1;
        if (trigger) count = pickWeightedInt({{1, 4200}, {2, 2800}, {3, 1700}, {4, 900}, {5, 350}, {6, 50}});
    } else if (state == NORMAL_FREE_SPINS) {
        // Source: Excel sheet "Normal_FS_Tables", Tables FS-A/FS-B.
        trigger = pickWeightedInt({{0, 4500}, {1, 5500}}) == 1;
        if (trigger) count = pickWeightedInt({{1, 4000}, {2, 3000}, {3, 1700}, {4, 900}, {5, 300}, {6, 100}});
    } else {
        // Source: Excel sheet "SuperSaw_FS_Tables", Tables SS-A/SS-B.
        trigger = pickWeightedInt({{0, 2500}, {1, 7500}}) == 1;
        if (trigger) count = pickWeightedInt({{1, 2500}, {2, 3000}, {3, 2300}, {4, 1400}, {5, 600}, {6, 200}});
    }

    if (!trigger) return;

    for (int i = 0; i < count; i++) {
        bool placed = false;
        for (int attempt = 0; attempt < 20 && !placed; attempt++) {
            int row = weightedRow(active_rows);
            int reel = weightedReel();
            if (row < startRow(active_rows)) continue;
            if (isHardHat(window[row][reel])) continue;

            window[row][reel] = state == BASE_GAME ? baseHardHatType(active_rows)
                                                   : featureHardHatType(state);
            placed = true;
        }
    }
}

int resolveHazardExpansion(const Window& window, int active_rows) {
    bool has_hazard = false;
    for (int row = startRow(active_rows); row < no_of_rows; row++) {
        for (int reel = 0; reel < no_of_reels; reel++) {
            if (window[row][reel] == HAZARD_HARD_HAT) has_hazard = true;
        }
    }
    if (!has_hazard || active_rows >= 6) return active_rows;

    // Source: Excel sheet "Base_Tables", Table D.
    // Ambiguity: Free-spin hazard unlock count is not separately specified.
    // Safest implementation reuses the only hazard unlock table in the model.
    int unlock = pickWeightedInt({{1, 7800}, {2, 1900}, {3, 300}});
    return min(6, active_rows + unlock);
}

// Source: Excel sheet "Base_Tables", Table E.
bool rollGirderTrigger(int hard_hat_count) {
    if (hard_hat_count <= 0 || hard_hat_count >= 6) return false;
    double p = getUniform();
    if (hard_hat_count == 1) return p < 0.0005;
    if (hard_hat_count == 2) return p < 0.0015;
    if (hard_hat_count == 3) return p < 0.0075;
    if (hard_hat_count == 4) return p < 0.03;
    return p < 0.18;
}

// Source: Excel sheet "Base_Tables", Table F.
Symbol girderCompletionSymbol() {
    return pickWeightedSymbol({
        {HARD_HAT, 8500},
        {HAZARD_HARD_HAT, 700},
        {HORIZONTAL_SAW_HARD_HAT, 300},
        {VERTICAL_SAW_HARD_HAT, 300},
        {SUPER_SAW_HARD_HAT, 200},
    });
}

void applyGirder(Window& window, int active_rows, Statistics& stats) {
    int hard_hat_count = countHardHats(window, active_rows);
    if (!rollGirderTrigger(hard_hat_count)) return;

    stats.girder_triggers++;
    int missing = max(0, 6 - hard_hat_count);
    vector<int> available;
    for (int row = startRow(active_rows); row < no_of_rows; row++) {
        for (int reel = 0; reel < no_of_reels; reel++) {
            if (!isHardHat(window[row][reel])) available.push_back(row * no_of_reels + reel);
        }
    }
    available = randomPositionsWithoutReplacement(available, min(missing, static_cast<int>(available.size())));
    for (int pos : available) {
        window[pos / no_of_reels][pos % no_of_reels] = girderCompletionSymbol();
    }
}

long long evaluateWaysWins(const Window&, int) {
    // TODO(Math model): Add actual line/ways paytable from the math model.
    // The supplied workbook includes RTP targets but not symbol paytable values.
    return 0;
}

long long awardOverflowPrize() {
    // Source: Excel sheet "Normal_FS_Tables", Table FS-D.
    double prize_xbet = pickWeightedDouble({
        {5.0, 4500},
        {10.0, 3000},
        {15.0, 1600},
        {25.0, 700},
        {50.0, 180},
        {100.0, 20},
    });
    return static_cast<long long>(prize_xbet * bet + 0.5);
}

long long awardHousePrize(Frame frame, long long& jackpot_win) {
    if (frame == STRAW_FRAME) {
        // Source: Excel sheet "Prize_Tables", Table P-A.
        return static_cast<long long>(pickWeightedDouble({{0.5, 5000}, {0.75, 3000}, {1.0, 2000}}) * bet + 0.5);
    }
    if (frame == STICK_FRAME) {
        // Source: Excel sheet "Prize_Tables", Table P-B.
        return static_cast<long long>(pickWeightedDouble({{1.5, 3500}, {2.0, 2500}, {3.0, 2000}, {4.5, 1500}, {7.5, 500}}) * bet + 0.5);
    }
    if (frame == BRICK_FRAME) {
        // Source: Excel sheet "Prize_Tables", Table P-C.
        vector<int> weights = {3500, 2500, 1600, 1000, 700, 400, 180, 90, 25, 5};
        int idx = weightedPick(weights);
        const double values[] = {7.5, 10, 15, 25, 50, 100, 25, 100, 500, 5000};
        long long win = static_cast<long long>(values[idx] * bet + 0.5);
        if (idx >= 6) jackpot_win += win;
        return win;
    }
    return 0;
}

vector<pair<int, int>> positionsWithFrame(const FrameWindow& frames, int active_rows, Frame frame) {
    vector<pair<int, int>> positions;
    for (int row = startRow(active_rows); row < no_of_rows; row++) {
        for (int reel = 0; reel < no_of_reels; reel++) {
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

    // Source: Game_Rules/Free_Game_1.json and Normal_FS_Tables FS-D.
    // Brick overflow priority: random Stick -> Brick, else random Straw -> Stick,
    // else random empty active cell -> Straw, else award overflow prize.
    auto stick_positions = positionsWithFrame(frames, active_rows, STICK_FRAME);
    if (!stick_positions.empty()) {
        auto pos = stick_positions[getRandom(0, static_cast<int>(stick_positions.size()) - 1)];
        frames[pos.first][pos.second] = BRICK_FRAME;
        return 0;
    }
    auto straw_positions = positionsWithFrame(frames, active_rows, STRAW_FRAME);
    if (!straw_positions.empty()) {
        auto pos = straw_positions[getRandom(0, static_cast<int>(straw_positions.size()) - 1)];
        frames[pos.first][pos.second] = STICK_FRAME;
        return 0;
    }
    auto empty_positions = positionsWithFrame(frames, active_rows, NO_FRAME);
    if (!empty_positions.empty()) {
        auto pos = empty_positions[getRandom(0, static_cast<int>(empty_positions.size()) - 1)];
        frames[pos.first][pos.second] = STRAW_FRAME;
        return 0;
    }
    return awardOverflowPrize();
}

long long applySawPath(FrameWindow& frames, int row, int reel, int active_rows, Symbol saw_symbol) {
    long long win = 0;
    vector<int> seen(no_of_rows * no_of_reels, 0);

    auto hit = [&](int r, int c) {
        int idx = r * no_of_reels + c;
        if (seen[idx]) return;
        seen[idx] = 1;
        win += applyFrameHit(frames, r, c, active_rows);
    };

    if (saw_symbol == HORIZONTAL_SAW_HARD_HAT || saw_symbol == SUPER_SAW_HARD_HAT) {
        for (int c = 0; c < no_of_reels; c++) {
            if (c != reel) hit(row, c);
        }
    }
    if (saw_symbol == VERTICAL_SAW_HARD_HAT || saw_symbol == SUPER_SAW_HARD_HAT) {
        for (int r = startRow(active_rows); r < no_of_rows; r++) {
            if (r != row) hit(r, reel);
        }
    }
    return win;
}

FeatureResult playFreeGames(GameState feature_state,
                            int starting_active_rows,
                            const vector<pair<int, int>>& trigger_positions,
                            const vector<ReelSet>& reelsets) {
    FeatureResult result;
    FrameWindow frames{};
    for (auto& row : frames) row.fill(NO_FRAME);

    int active_rows = starting_active_rows;
    int remaining_spins = 6;

    for (const auto& pos : trigger_positions) {
        frames[pos.first][pos.second] = STRAW_FRAME;
    }

    while (remaining_spins > 0) {
        result.spins_played++;
        const ReelSet& rs = findReelSet(reelsets, feature_state, active_rows);
        Window window = generateWindow(rs);
        placeHardHats(window, feature_state, active_rows);
        active_rows = resolveHazardExpansion(window, active_rows);

        vector<pair<int, int>> hh_positions = hardHatPositions(window, active_rows);
        int retrigger_symbols = 0;

        for (const auto& pos : hh_positions) {
            Symbol s = window[pos.first][pos.second];
            result.overflow_win += applyFrameHit(frames, pos.first, pos.second, active_rows);
            if (feature_state == NORMAL_FREE_SPINS) {
                retrigger_symbols++;
            } else if (isSawHat(s) || s == HAZARD_HARD_HAT) {
                retrigger_symbols++;
            }
        }

        // Source: Game_Rules/Free_Spins_1.json and Free_Spins_2.json.
        // Saws are generated after all other frame updates have occurred.
        for (const auto& pos : hh_positions) {
            Symbol s = window[pos.first][pos.second];
            if (isSawHat(s)) {
                result.overflow_win += applySawPath(frames, pos.first, pos.second, active_rows, s);
            }
        }

        if (retrigger_symbols >= 3) {
            remaining_spins++;
            result.retriggers++;
        }
        remaining_spins--;
    }

    for (int row = 0; row < no_of_rows; row++) {
        for (int reel = 0; reel < no_of_reels; reel++) {
            if (frames[row][reel] != NO_FRAME) {
                result.house_prizes++;
                result.total_win += awardHousePrize(frames[row][reel], result.jackpot_win);
            }
        }
    }
    result.total_win += result.overflow_win;
    return result;
}

SpinResult spinBaseGame(const vector<ReelSet>& reelsets, Statistics& stats) {
    SpinResult result;
    int active_rows = 3;

    const ReelSet& rs = findReelSet(reelsets, BASE_GAME, active_rows);
    Window window = generateWindow(rs);
    stats.reelset_usage_base[active_rows - 3]++;

    applyMysteryStack(window, active_rows, stats);
    placeHardHats(window, BASE_GAME, active_rows);
    active_rows = resolveHazardExpansion(window, active_rows);

    result.base_win = evaluateWaysWins(window, active_rows);

    applyGirder(window, active_rows, stats);
    active_rows = resolveHazardExpansion(window, active_rows);
    result.active_rows = active_rows;

    int trigger_count = countHardHats(window, active_rows);
    if (trigger_count >= 6) {
        vector<pair<int, int>> trigger_positions = hardHatPositions(window, active_rows);
        bool super_saw = hasSuperSawHat(window, active_rows);
        FeatureResult feature = playFreeGames(super_saw ? SUPER_SAW_FREE_SPINS : NORMAL_FREE_SPINS,
                                             active_rows,
                                             trigger_positions,
                                             reelsets);
        long long capped_total = min(max_win_credits, result.base_win + feature.total_win);
        feature.total_win = max(0LL, capped_total - result.base_win);
        result.feature_win = feature.total_win;
        result.overflow_win = feature.overflow_win;
        result.jackpot_win = feature.jackpot_win;
        stats.feature_spins += feature.spins_played;
        stats.retriggers += feature.retriggers;

        if (super_saw) {
            result.triggered_super_fs = true;
            stats.super_saw_fs_triggers++;
            stats.super_saw_fs_win += feature.total_win;
            stats.reelset_usage_super[active_rows - 3]++;
        } else {
            result.triggered_normal_fs = true;
            stats.normal_fs_triggers++;
            stats.normal_fs_win += feature.total_win;
            stats.reelset_usage_normal[active_rows - 3]++;
        }
    }

    long long total = result.base_win + result.feature_win;
    result.hit = total > 0;
    return result;
}

void updateStatistics(Statistics& stats, const SpinResult& result) {
    long long total = result.base_win + result.feature_win;
    stats.spins++;
    stats.total_bet += bet;
    stats.total_win += total;
    stats.base_ways_win += result.base_win;
    stats.overflow_win += result.overflow_win;
    stats.jackpot_win += result.jackpot_win;
    stats.max_win = max(stats.max_win, total);
    stats.win_sq_sum += static_cast<double>(total) * static_cast<double>(total);
    if (total > 0) stats.hits++;
    if (result.base_win > 0) stats.base_hits++;
}

double rtp(long long win, const Statistics& stats) {
    if (stats.total_bet == 0) return 0.0;
    return static_cast<double>(win) / static_cast<double>(stats.total_bet);
}

double frequency(long long events, long long trials) {
    if (events == 0 || trials == 0) return 0.0;
    return static_cast<double>(trials) / static_cast<double>(events);
}

string formatPercent(double value) {
    ostringstream out;
    out << fixed << setprecision(4) << value * 100.0 << "%";
    return out.str();
}

void printResults(const Statistics& stats, const GameConfig& config, ostream& out) {
    double mean = stats.spins > 0 ? static_cast<double>(stats.total_win) / stats.spins : 0.0;
    double second_moment = stats.spins > 0 ? stats.win_sq_sum / stats.spins : 0.0;
    double variance = max(0.0, second_moment - mean * mean);
    double stddev = sqrt(variance) / bet;

    out << "# RTP Summary\n\n";
    out << "## Run Settings\n\n";
    out << "- Total spins simulated: " << stats.spins << "\n";
    out << "- Bet per spin: " << bet << "\n";
    out << "- Seed: " << config.seed << "\n";
    out << "- Source model: `Huff_N_Puff_Highrise_Math_Model_12_Reelsets.xlsx`\n";
    out << "- Source rules: `Game_Rules/*.json`\n\n";

    out << "## RTP\n\n";
    out << "| Component | Credits | RTP |\n";
    out << "|---|---:|---:|\n";
    out << "| Total bet | " << stats.total_bet << " | |\n";
    out << "| Total win | " << stats.total_win << " | " << formatPercent(rtp(stats.total_win, stats)) << " |\n";
    out << "| Base ways wins | " << stats.base_ways_win << " | " << formatPercent(rtp(stats.base_ways_win, stats)) << " |\n";
    out << "| Normal free spins | " << stats.normal_fs_win << " | " << formatPercent(rtp(stats.normal_fs_win, stats)) << " |\n";
    out << "| Super Saw free spins | " << stats.super_saw_fs_win << " | " << formatPercent(rtp(stats.super_saw_fs_win, stats)) << " |\n";
    out << "| Overflow prizes | " << stats.overflow_win << " | " << formatPercent(rtp(stats.overflow_win, stats)) << " |\n";
    out << "| Jackpot prizes | " << stats.jackpot_win << " | " << formatPercent(rtp(stats.jackpot_win, stats)) << " |\n\n";

    out << "## Frequencies\n\n";
    out << "| Metric | Count | Frequency |\n";
    out << "|---|---:|---:|\n";
    out << "| Any hit | " << stats.hits << " | 1 in " << fixed << setprecision(2) << frequency(stats.hits, stats.spins) << " |\n";
    out << "| Base hit | " << stats.base_hits << " | 1 in " << frequency(stats.base_hits, stats.spins) << " |\n";
    out << "| Normal FS trigger | " << stats.normal_fs_triggers << " | 1 in " << frequency(stats.normal_fs_triggers, stats.spins) << " |\n";
    out << "| Super Saw FS trigger | " << stats.super_saw_fs_triggers << " | 1 in " << frequency(stats.super_saw_fs_triggers, stats.spins) << " |\n";
    out << "| Any feature trigger | " << (stats.normal_fs_triggers + stats.super_saw_fs_triggers) << " | 1 in " << frequency(stats.normal_fs_triggers + stats.super_saw_fs_triggers, stats.spins) << " |\n";
    out << "| Girder trigger | " << stats.girder_triggers << " | 1 in " << frequency(stats.girder_triggers, stats.spins) << " |\n";
    out << "| Mystery Stack trigger | " << stats.mystery_stack_triggers << " | 1 in " << frequency(stats.mystery_stack_triggers, stats.spins) << " |\n";
    out << "| Feature retriggers | " << stats.retriggers << " | |\n";
    out << "| Feature spins played | " << stats.feature_spins << " | |\n\n";

    long long feature_triggers = stats.normal_fs_triggers + stats.super_saw_fs_triggers;
    double avg_feature_win = feature_triggers > 0
        ? static_cast<double>(stats.normal_fs_win + stats.super_saw_fs_win) / feature_triggers / bet
        : 0.0;

    out << "## Volatility And Limits\n\n";
    out << "- Average feature win: " << fixed << setprecision(4) << avg_feature_win << "x bet\n";
    out << "- Maximum win observed: " << static_cast<double>(stats.max_win) / bet << "x bet\n";
    out << "- Max win cap configured: " << max_win_xbet << "x bet\n";
    out << "- Standard deviation estimate: " << stddev << "x bet\n\n";

    out << "## Validation Counters\n\n";
    out << "- Base reelset usage R1-R4: "
        << stats.reelset_usage_base[0] << ", "
        << stats.reelset_usage_base[1] << ", "
        << stats.reelset_usage_base[2] << ", "
        << stats.reelset_usage_base[3] << "\n";
    out << "- Normal FS reelset usage R5-R8: "
        << stats.reelset_usage_normal[0] << ", "
        << stats.reelset_usage_normal[1] << ", "
        << stats.reelset_usage_normal[2] << ", "
        << stats.reelset_usage_normal[3] << "\n";
    out << "- Super Saw FS reelset usage R9-R12: "
        << stats.reelset_usage_super[0] << ", "
        << stats.reelset_usage_super[1] << ", "
        << stats.reelset_usage_super[2] << ", "
        << stats.reelset_usage_super[3] << "\n\n";

    out << "## Assumptions And TODOs\n\n";
    out << "- TODO: Add final reel strips. Workbook sheet `Reelsets_12` currently provides placeholder strip counts only.\n";
    out << "- TODO: Add symbol paytable. The workbook does not provide line/ways pay values, so `evaluateWaysWins()` returns zero.\n";
    out << "- Hazard row unlock in free spins reuses Base Table D because no separate free-spin hazard table is supplied.\n";
    out << "- Placeholder strip construction uses balanced paying symbols plus workbook wild/HH counts and is not final math validation data.\n";
}

GameConfig parseArgs(int argc, char** argv) {
    GameConfig config;
    config.seed = static_cast<unsigned int>(
        std::chrono::high_resolution_clock::now().time_since_epoch().count()
    );

    for (int i = 1; i < argc; i++) {
        string arg = argv[i];
        if (arg == "--spins" && i + 1 < argc) {
            config.default_spins = stoi(argv[++i]);
        } else if (arg == "--seed" && i + 1 < argc) {
            config.seed = static_cast<unsigned int>(stoul(argv[++i]));
        } else if (arg == "--output" && i + 1 < argc) {
            config.output_path = argv[++i];
        } else if (arg == "--help") {
            cout << "Usage: ./core_new [--spins N] [--seed N] [--output outputs/RTP_summary.md]\n";
            exit(0);
        }
    }
    return config;
}

int main(int argc, char** argv) {
    GameConfig config = parseArgs(argc, argv);
    rng.seed(config.seed);

    vector<ReelSet> reelsets = loadTables();
    Statistics stats;

    for (int i = 0; i < config.default_spins; i++) {
        SpinResult result = spinBaseGame(reelsets, stats);
        updateStatistics(stats, result);
    }

    printResults(stats, config, cout);

    std::filesystem::path out_path(config.output_path);
    if (out_path.has_parent_path()) {
        std::filesystem::create_directories(out_path.parent_path());
    }
    ofstream file(config.output_path);
    if (file.is_open()) {
        printResults(stats, config, file);
    }

    return 0;
}
