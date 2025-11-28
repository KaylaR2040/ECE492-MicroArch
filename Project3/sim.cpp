#include <array>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr int NUM_REGS = 67;

enum Stage {
    FE = 0,
    DE,
    RN,
    RR,
    DI,
    IS,
    EX,
    WB,
    RT,
    STAGE_COUNT
};

constexpr const char* STAGE_NAMES[STAGE_COUNT] = {"FE", "DE", "RN", "RR", "DI", "IS", "EX", "WB", "RT"};

struct Instruction {
    uint64_t seq_no = 0;
    uint64_t pc = 0;
    int op_type = 0;
    int dest = -1;
    int src1 = -1;
    int src2 = -1;
    int rob_index = -1;
    int src_tag[2] = {-1, -1};
    bool src_ready[2] = {false, false};
    bool stage_started[STAGE_COUNT] = {false};
    uint64_t stage_begin[STAGE_COUNT] = {0};
    uint64_t stage_duration[STAGE_COUNT] = {0};

    Instruction(uint64_t seq, uint64_t pc_val, int op, int d, int s1, int s2)
        : seq_no(seq), pc(pc_val), op_type(op), dest(d), src1(s1), src2(s2) {}

    void enter_stage(Stage stage, uint64_t cycle) {
        if (!stage_started[stage]) {
            stage_started[stage] = true;
            stage_begin[stage] = cycle;
        }
    }

    void leave_stage(Stage stage, uint64_t cycle) {
        stage_duration[stage] = cycle - stage_begin[stage] + 1;
    }
};

struct ROBEntry {
    bool valid = false;
    bool ready = false;
    Instruction* inst = nullptr;
};

struct IQEntry {
    bool valid = false;
    Instruction* inst = nullptr;
};

struct ExecRecord {
    Instruction* inst = nullptr;
    uint64_t done_cycle = 0;
};

struct Config {
    int rob_size = 0;
    int iq_size = 0;
    int width = 0;
};

class Simulator {
  public:
    Simulator(const Config& cfg, const std::string& trace_path)
        : config_(cfg), trace_path_(trace_path) {
        trace_fp_ = std::fopen(trace_path_.c_str(), "r");
        if (!trace_fp_) {
            std::perror("Error opening trace file");
            std::exit(EXIT_FAILURE);
        }
        rob_.resize(config_.rob_size);
        iq_entries_.resize(config_.iq_size);
        rmt_.assign(NUM_REGS, -1);
    }

    ~Simulator() {
        if (trace_fp_) {
            std::fclose(trace_fp_);
        }
        for (Instruction* inst : all_insts_) {
            delete inst;
        }
    }

    void run() {
        bool cont = true;
        do {
            prepare_next_state();
            retire();
            writeback();
            execute();
            issue();
            dispatch();
            regread();
            rename();
            decode();
            fetch();
            cont = advance_cycle();
        } while (cont);
    }

    void print_results() const {
        for (const Instruction* inst : all_insts_) {
            std::cout << inst->seq_no << " fu{" << inst->op_type << "} src{" << inst->src1 << "," << inst->src2
                      << "} dst{" << inst->dest << "}";
            for (int i = 0; i < STAGE_COUNT; ++i) {
                std::cout << " " << STAGE_NAMES[i] << "{" << inst->stage_begin[i] << "," << inst->stage_duration[i]
                          << "}";
            }
            std::cout << "\n";
        }
        std::cout << "# === Simulator Command =========\n";
        std::string trace_name = trace_path_;
        size_t pos = trace_name.find_last_of("/\\");
        if (pos != std::string::npos) {
            trace_name = trace_name.substr(pos + 1);
        }
        std::cout << "# ./sim " << config_.rob_size << " " << config_.iq_size << " " << config_.width << " "
                  << trace_name << "\n";
        std::cout << "# === Processor Configuration ===\n";
        std::cout << "# ROB_SIZE = " << config_.rob_size << "\n";
        std::cout << "# IQ_SIZE  = " << config_.iq_size << "\n";
        std::cout << "# WIDTH    = " << config_.width << "\n";
        std::cout << "# === Simulation Results ========\n";
        std::cout << "# Dynamic Instruction Count    = " << retired_count_ << "\n";
        std::cout << "# Cycles                       = " << cycle_ << "\n";
        double ipc = (cycle_ == 0) ? 0.0 : static_cast<double>(retired_count_) / static_cast<double>(cycle_);
        std::cout << "# Instructions Per Cycle (IPC) = " << std::fixed << std::setprecision(2) << ipc << "\n";
    }

  private:
    void prepare_next_state() {
        de_next_ = de_curr_;
        rn_next_ = rn_curr_;
        rr_next_ = rr_curr_;
        di_next_ = di_curr_;
        wb_next_.clear();
    }

    void retire() {
        int retired_this_cycle = 0;
        while (retired_this_cycle < config_.width && rob_count_ > 0) {
            ROBEntry& head = rob_[rob_head_];
            if (!head.valid || !head.ready) {
                break;
            }
            Instruction* inst = head.inst;
            if (!inst->stage_started[Stage::RT] || cycle_ < inst->stage_begin[Stage::RT]) {
                break;
            }
            inst->leave_stage(Stage::RT, cycle_);
            head.valid = false;
            head.ready = false;
            head.inst = nullptr;
            if (inst->dest >= 0 && rmt_[inst->dest] == inst->rob_index) {
                rmt_[inst->dest] = -1;
            }
            rob_head_ = (rob_head_ + 1) % config_.rob_size;
            --rob_count_;
            ++retired_this_cycle;
            ++retired_count_;
        }
    }

    void writeback() {
        for (Instruction* inst : wb_curr_) {
            inst->leave_stage(Stage::WB, cycle_);
            ROBEntry& entry = rob_[inst->rob_index];
            entry.ready = true;
            inst->enter_stage(Stage::RT, cycle_ + 1);
        }
        wb_curr_.clear();
    }

    void wakeup_dependents(int rob_index) {
        auto update = [rob_index](Instruction* inst) {
            for (int i = 0; i < 2; ++i) {
                if (!inst->src_ready[i] && inst->src_tag[i] == rob_index) {
                    inst->src_ready[i] = true;
                }
            }
        };
        for (IQEntry& entry : iq_entries_) {
            if (entry.valid) {
                update(entry.inst);
            }
        }
        for (Instruction* inst : di_curr_) {
            update(inst);
        }
        for (Instruction* inst : di_next_) {
            update(inst);
        }
        for (Instruction* inst : rr_curr_) {
            update(inst);
        }
        for (Instruction* inst : rr_next_) {
            update(inst);
        }
    }

    void execute() {
        std::vector<ExecRecord> remaining;
        remaining.reserve(execute_list_.size());
        for (const ExecRecord& rec : execute_list_) {
            if (rec.done_cycle <= cycle_) {
                Instruction* inst = rec.inst;
                inst->leave_stage(Stage::EX, cycle_);
                inst->enter_stage(Stage::WB, cycle_ + 1);
                ROBEntry& entry = rob_[inst->rob_index];
                entry.ready = true;
                wb_next_.push_back(inst);
                wakeup_dependents(inst->rob_index);
            } else {
                remaining.push_back(rec);
            }
        }
        execute_list_.swap(remaining);
    }

    static int latency_for_op(int op_type) {
        switch (op_type) {
            case 0:
                return 1;
            case 1:
                return 2;
            default:
                return 5;
        }
    }

    void issue() {
        int issued = 0;
        while (issued < config_.width) {
            int best_idx = -1;
            uint64_t best_seq = std::numeric_limits<uint64_t>::max();
            for (size_t i = 0; i < iq_entries_.size(); ++i) {
                IQEntry& entry = iq_entries_[i];
                if (!entry.valid) {
                    continue;
                }
                Instruction* inst = entry.inst;
                if (inst->src_ready[0] && inst->src_ready[1]) {
                    if (inst->seq_no < best_seq) {
                        best_seq = inst->seq_no;
                        best_idx = static_cast<int>(i);
                    }
                }
            }
            if (best_idx == -1) {
                break;
            }
            IQEntry& entry = iq_entries_[best_idx];
            Instruction* inst = entry.inst;
            entry.valid = false;
            entry.inst = nullptr;
            --iq_occupancy_;
            inst->leave_stage(Stage::IS, cycle_);
            inst->enter_stage(Stage::EX, cycle_ + 1);
            int latency = latency_for_op(inst->op_type);
            execute_list_.push_back({inst, cycle_ + latency});
            ++issued;
        }
    }

    void dispatch() {
        if (di_curr_.empty()) {
            di_next_.clear();
            return;
        }
        size_t needed = di_curr_.size();
        size_t free_entries = config_.iq_size - iq_occupancy_;
        if (free_entries < needed) {
            di_next_ = di_curr_;
            return;
        }
        for (Instruction* inst : di_curr_) {
            inst->leave_stage(Stage::DI, cycle_);
            inst->enter_stage(Stage::IS, cycle_ + 1);
            insert_into_iq(inst);
        }
        di_next_.clear();
    }

    void regread() {
        if (rr_curr_.empty()) {
            rr_next_.clear();
            return;
        }
        if (!di_next_.empty()) {
            rr_next_ = rr_curr_;
            return;
        }
        for (Instruction* inst : rr_curr_) {
            inst->leave_stage(Stage::RR, cycle_);
            inst->enter_stage(Stage::DI, cycle_ + 1);
        }
        di_next_ = rr_curr_;
        rr_next_.clear();
    }

    void rename() {
        if (rn_curr_.empty()) {
            rn_next_.clear();
            return;
        }
        size_t needed = rn_curr_.size();
        if (!rr_next_.empty() || static_cast<int>(needed) > (config_.rob_size - rob_count_)) {
            rn_next_ = rn_curr_;
            return;
        }
        for (Instruction* inst : rn_curr_) {
            inst->leave_stage(Stage::RN, cycle_);
            inst->enter_stage(Stage::RR, cycle_ + 1);
            int rob_index = rob_tail_;
            ROBEntry& entry = rob_[rob_index];
            entry.valid = true;
            entry.ready = false;
            entry.inst = inst;
            inst->rob_index = rob_index;
            rob_tail_ = (rob_tail_ + 1) % config_.rob_size;
            ++rob_count_;

            const int src_regs[2] = {inst->src1, inst->src2};
            for (int i = 0; i < 2; ++i) {
                int reg = src_regs[i];
                if (reg < 0) {
                    inst->src_ready[i] = true;
                    inst->src_tag[i] = -1;
                    continue;
                }
                int tag = rmt_[reg];
                inst->src_tag[i] = tag;
                if (tag == -1) {
                    inst->src_ready[i] = true;
                } else {
                    inst->src_ready[i] = rob_[tag].ready;
                }
            }

            if (inst->dest >= 0) {
                rmt_[inst->dest] = inst->rob_index;
            }
        }
        rr_next_ = rn_curr_;
        rn_next_.clear();
    }

    void decode() {
        if (de_curr_.empty()) {
            de_next_.clear();
            return;
        }
        if (!rn_next_.empty()) {
            de_next_ = de_curr_;
            return;
        }
        for (Instruction* inst : de_curr_) {
            inst->leave_stage(Stage::DE, cycle_);
            inst->enter_stage(Stage::RN, cycle_ + 1);
        }
        rn_next_ = de_curr_;
        de_next_.clear();
    }

    bool read_trace(uint64_t& pc, int& op, int& dst, int& s1, int& s2) {
        if (trace_eof_) {
            return false;
        }
        unsigned long long pc_tmp = 0;
        int ret = std::fscanf(trace_fp_, "%llx %d %d %d %d", &pc_tmp, &op, &dst, &s1, &s2);
        if (ret == 5) {
            pc = static_cast<uint64_t>(pc_tmp);
            return true;
        }
        trace_eof_ = true;
        return false;
    }

    void fetch() {
        if (trace_eof_) {
            return;
        }
        if (!de_next_.empty()) {
            return;
        }
        std::vector<Instruction*> bundle;
        bundle.reserve(config_.width);
        for (int i = 0; i < config_.width; ++i) {
            uint64_t pc = 0;
            int op = 0, dst = -1, s1 = -1, s2 = -1;
            if (!read_trace(pc, op, dst, s1, s2)) {
                break;
            }
            Instruction* inst = new Instruction(seq_counter_++, pc, op, dst, s1, s2);
            inst->enter_stage(Stage::FE, cycle_);
            inst->leave_stage(Stage::FE, cycle_);
            inst->enter_stage(Stage::DE, cycle_ + 1);
            all_insts_.push_back(inst);
            bundle.push_back(inst);
        }
        if (!bundle.empty()) {
            de_next_ = bundle;
        } else {
            trace_eof_ = true;
        }
    }

    void insert_into_iq(Instruction* inst) {
        for (IQEntry& entry : iq_entries_) {
            if (!entry.valid) {
                entry.valid = true;
                entry.inst = inst;
                ++iq_occupancy_;
                return;
            }
        }
        std::cerr << "IQ overflow detected\n";
        std::exit(EXIT_FAILURE);
    }

    bool advance_cycle() {
        ++cycle_;
        de_curr_ = de_next_;
        rn_curr_ = rn_next_;
        rr_curr_ = rr_next_;
        di_curr_ = di_next_;
        wb_curr_ = wb_next_;
        wb_next_.clear();
        bool pipeline_empty = de_curr_.empty() && rn_curr_.empty() && rr_curr_.empty() && di_curr_.empty() &&
                              execute_list_.empty() && wb_curr_.empty() && iq_occupancy_ == 0 && rob_count_ == 0;
        return !(pipeline_empty && trace_eof_);
    }

    Config config_;
    std::string trace_path_;
    FILE* trace_fp_ = nullptr;
    bool trace_eof_ = false;

    uint64_t cycle_ = 0;
    uint64_t seq_counter_ = 0;
    uint64_t retired_count_ = 0;

    std::vector<Instruction*> de_curr_, de_next_;
    std::vector<Instruction*> rn_curr_, rn_next_;
    std::vector<Instruction*> rr_curr_, rr_next_;
    std::vector<Instruction*> di_curr_, di_next_;
    std::vector<Instruction*> wb_curr_, wb_next_;

    std::vector<IQEntry> iq_entries_;
    int iq_occupancy_ = 0;

    std::vector<ROBEntry> rob_;
    int rob_head_ = 0;
    int rob_tail_ = 0;
    int rob_count_ = 0;

    std::vector<int> rmt_;
    std::vector<ExecRecord> execute_list_;
    std::vector<Instruction*> all_insts_;
};

}  // namespace

int main(int argc, char* argv[]) {
    if (argc != 5) {
        std::cerr << "Usage: " << argv[0] << " <ROB_SIZE> <IQ_SIZE> <WIDTH> <tracefile>\n";
        return EXIT_FAILURE;
    }
    Config cfg{};
    cfg.rob_size = std::stoi(argv[1]);
    cfg.iq_size = std::stoi(argv[2]);
    cfg.width = std::stoi(argv[3]);
    if (cfg.rob_size <= 0 || cfg.iq_size <= 0 || cfg.width <= 0) {
        std::cerr << "All parameters must be positive integers.\n";
        return EXIT_FAILURE;
    }
    Simulator sim(cfg, argv[4]);
    sim.run();
    sim.print_results();
    return 0;
}
