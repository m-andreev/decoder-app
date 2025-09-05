#include <algorithm>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

struct AsmState {
    size_t exp_len = 0;          // expected total payload length
    std::vector<uint8_t> buf;    // accumulated payload (excluding PCI)
    bool active = false;
};

static bool hexpair(const std::string &s, size_t i, uint8_t &out) {
    if (i + 2 > s.size()) return false;
    auto hexv = [](char c)->int {
        if ('0'<=c && c<='9') return c-'0';
        if ('a'<=c && c<='f') return 10 + (c-'a');
        if ('A'<=c && c<='F') return 10 + (c-'A');
        return -1;
    };
    int a = hexv(s[i]), b = hexv(s[i+1]);
    if (a<0 || b<0) return false;
    out = static_cast<uint8_t>((a<<4) | b);
    return true;
}

static bool parse_payload_bytes(const std::string &hex, std::vector<uint8_t> &out) {
    if (hex.size() % 2 != 0) return false;
    out.clear();
    out.reserve(hex.size()/2);
    for (size_t i=0; i<hex.size(); i+=2) {
        uint8_t v;
        if (!hexpair(hex, i, v)) return false;
        out.push_back(v);
    }
    return true;
}

static std::string to_hex(const std::vector<uint8_t>& v, size_t upto = SIZE_MAX) {
    static const char* H = "0123456789ABCDEF";
    std::string s; s.reserve((std::min(v.size(),upto))*2);
    size_t n = std::min(v.size(), upto);
    for (size_t i=0; i<n; ++i) {
        s.push_back(H[(v[i]>>4)&0xF]);
        s.push_back(H[v[i]&0xF]);
    }
    return s;
}

// Human-readable STmin per ISO-TP (0x00..0x7F ms; 0xF1..0xF9 = 100..900 us)
static std::string stmin_to_string(uint8_t st) {
    if (st <= 0x7F) {
        return std::to_string((unsigned)st) + "ms";
    } else if (st >= 0xF1 && st <= 0xF9) {
        int us = (st - 0xF0) * 100; // 0xF1=100us ... 0xF9=900us
        return std::to_string(us) + "us";
    } else {
        char buf[8];
        std::snprintf(buf, sizeof(buf), "0x%02X", st);
        return std::string(buf);
    }
}

int main(int argc, char** argv) {
    using namespace std;

    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    // flags
    bool show_fc = true; // по подразбиране вече показваме FC; можеш да изключиш с --no-fc
    for (int i=1; i<argc; ++i) {
        string a = argv[i];
        if (a == "--no-fc") show_fc = false;
    }

    string inputPath = (argc >= 2 && string(argv[argc-1]).rfind("--",0)!=0)
                       ? argv[argc-1]
                       : "transcript.txt";

    ifstream fin(inputPath);
    if (!fin) {
        cerr << "Failed to open input file: " << inputPath << "\n";
        return 1;
    }

    unordered_map<string, AsmState> state;

    string line;
    while (getline(fin, line)) {
        // trim spaces and control chars
        line.erase(remove_if(line.begin(), line.end(), [](unsigned char c){
            return c=='\r' || c=='\n' || isspace(c);
        }), line.end());
        if (line.empty()) continue;

        // Expect 3 hex for CAN ID + 16 hex for data (8 bytes)
        if (line.size() < 19) continue; // skip malformed
        string can_id = line.substr(0, 3);
        string payload_hex = line.substr(3);

        vector<uint8_t> data;
        if (!parse_payload_bytes(payload_hex, data) || data.size() != 8) continue;

        uint8_t pci = data[0];
        uint8_t pci_type = (pci >> 4) & 0xF;

        if (pci_type == 0x0) { // Single Frame
            uint8_t len = pci & 0x0F;
            vector<uint8_t> msg;
            msg.reserve(len);
            for (int i=1; i<8 && (int)msg.size()<len; ++i) msg.push_back(data[i]);
            cout << can_id << ": " << to_hex(msg) << "\n"; // chronological
            state.erase(can_id); // clear dangling state if any
        } else if (pci_type == 0x1) { // First Frame
            size_t total_len = ((size_t)(pci & 0x0F) << 8) | data[1];
            AsmState &st = state[can_id];
            st.active = true;
            st.exp_len = total_len;
            st.buf.clear();
            for (int i=2; i<8; ++i) st.buf.push_back(data[i]);
        } else if (pci_type == 0x2) { // Consecutive Frame
            auto it = state.find(can_id);
            if (it == state.end() || !it->second.active) continue;
            AsmState &st = it->second;
            for (int i=1; i<8; ++i) st.buf.push_back(data[i]);
            if (st.buf.size() >= st.exp_len) {
                cout << can_id << ": " << to_hex(st.buf, st.exp_len) << "\n"; // chronological
                state.erase(it);
            }
        } else if (pci_type == 0x3) { // Flow Control
            if (!show_fc) continue;
            uint8_t fs = pci & 0x0F; // FlowStatus
            uint8_t bs = data[1];    // BlockSize
            uint8_t stmin = data[2]; // Separation Time minimum

            const char* fs_text = nullptr;
            switch (fs) {
                case 0x0: fs_text = "CTS"; break;        // Continue To Send
                case 0x1: fs_text = "Wait"; break;
                case 0x2: fs_text = "Overflow"; break;   // /Abort
                default:  fs_text = "Unknown";
            }

            // Печатаме кратко, но информативно
            cout << can_id << ": "
                 << "FC(" << fs_text
                 << ", BS=" << std::dec << (unsigned)bs
                 << ", STmin=" << stmin_to_string(stmin)
                 << ")\n";
        } else {
            continue; // Unknown PCI
        }
    }
    return 0;
}
