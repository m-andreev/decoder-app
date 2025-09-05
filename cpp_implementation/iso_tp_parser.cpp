#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cstdint>

struct CANFrame {
    uint16_t can_id;
    std::vector<uint8_t> data;
};

struct ISOTPMessage {
    uint16_t ecu_header;
    std::vector<uint8_t> payload;
    bool complete;
    uint16_t expected_length;
    uint8_t next_sequence;
    
    ISOTPMessage() : complete(false), expected_length(0), next_sequence(1) {}
};

class ISOTPParser {
private:
    std::map<uint32_t, ISOTPMessage> messages;
    std::vector<ISOTPMessage> completed_messages;
    
    CANFrame parseCANFrame(const std::string& line) {
        CANFrame frame;
        
        if (line.length() < 3) {
            frame.can_id = 0;
            return frame;
        }
        
        // Extract CAN ID (first 3 hex characters)
        std::string can_id_str = line.substr(0, 3);
        frame.can_id = std::stoul(can_id_str, nullptr, 16);
        
        // Extract data bytes (remaining characters, 2 chars per byte)
        std::string data_str = line.substr(3);
        for (size_t i = 0; i < data_str.length(); i += 2) {
            if (i + 1 < data_str.length()) {
                std::string byte_str = data_str.substr(i, 2);
                uint8_t byte_val = std::stoul(byte_str, nullptr, 16);
                frame.data.push_back(byte_val);
            }
        }
        
        return frame;
    }
    
    void processSingleFrame(const CANFrame& frame) {
        if (frame.data.empty()) return;

        uint8_t pci = frame.data[0];
        uint8_t length = pci & 0x0F;

        if (length == 0 || length > 7 || frame.data.size() < static_cast<size_t>(length + 1)) {
            return;
        }

        ISOTPMessage message;
        message.ecu_header = frame.can_id;
        message.complete = true;

        // Extract payload (skip PCI byte)
        for (int i = 1; i <= length; i++) {
            if (static_cast<size_t>(i) < frame.data.size()) {
                message.payload.push_back(frame.data[i]);
            }
        }

        // Add completed single frame message to the list
        completed_messages.push_back(message);
    }
    
    void processFirstFrame(const CANFrame& frame) {
        if (frame.data.size() < 2) return;

        uint8_t pci_high = frame.data[0];
        uint8_t pci_low = frame.data[1];

        // Extract 12-bit length from first frame
        uint16_t length = ((pci_high & 0x0F) << 8) | pci_low;

        if (length == 0) return;

        uint32_t key = frame.can_id;
        ISOTPMessage& message = messages[key];
        message.ecu_header = frame.can_id;
        message.expected_length = length;
        message.complete = false;
        message.next_sequence = 1;
        message.payload.clear();

        // Extract initial payload (skip 2 PCI bytes)
        for (size_t i = 2; i < frame.data.size(); i++) {
            message.payload.push_back(frame.data[i]);
        }
    }
    
    void processConsecutiveFrame(const CANFrame& frame) {
        if (frame.data.empty()) return;

        uint8_t pci = frame.data[0];
        uint8_t sequence = pci & 0x0F;

        uint32_t key = frame.can_id;
        auto it = messages.find(key);
        if (it == messages.end() || it->second.complete) {
            return;
        }

        ISOTPMessage& message = it->second;

        // Check sequence number (allow wrapping from F to 0)
        if (sequence != message.next_sequence) {
            // Try to handle out-of-order frames or reset sequence
            // For now, just continue processing
        }

        // Add payload data (skip PCI byte)
        for (size_t i = 1; i < frame.data.size(); i++) {
            if (message.payload.size() < message.expected_length) {
                message.payload.push_back(frame.data[i]);
            }
        }

        // Update next expected sequence (wrap from F to 0)
        message.next_sequence = (sequence + 1) & 0x0F;
        if (message.next_sequence == 0) {
            message.next_sequence = 1; // After F comes 0, but we use 1-F
        }

        // Check if message is complete
        if (message.payload.size() >= message.expected_length) {
            message.complete = true;
            // Add completed multi-frame message to the list
            completed_messages.push_back(message);
        }
    }
    
    void processFlowControlFrame(const CANFrame& /* frame */) {
        // Flow control frames are typically sent by receivers
        // For this parser, we just ignore them as we're reconstructing messages
    }
    
public:
    void processFrame(const std::string& line) {
        if (line.empty()) return;
        
        CANFrame frame = parseCANFrame(line);
        if (frame.data.empty()) return;
        
        uint8_t pci = frame.data[0];
        uint8_t frame_type = (pci >> 4) & 0x0F;
        
        switch (frame_type) {
            case 0: // Single Frame
                processSingleFrame(frame);
                break;
            case 1: // First Frame
                processFirstFrame(frame);
                break;
            case 2: // Consecutive Frame
                processConsecutiveFrame(frame);
                break;
            case 3: // Flow Control Frame
                processFlowControlFrame(frame);
                break;
            default:
                // Unknown frame type
                break;
        }
    }
    
    void printCompletedMessages(std::ostream& output) {
        // Sort completed messages by CAN ID for consistent output
        std::map<uint16_t, std::vector<ISOTPMessage>> messages_by_id;

        for (const auto& message : completed_messages) {
            if (!message.payload.empty()) {
                messages_by_id[message.ecu_header].push_back(message);
            }
        }

        for (const auto& id_pair : messages_by_id) {
            for (const auto& message : id_pair.second) {
                output << std::hex << std::uppercase << id_pair.first << ": ";

                for (size_t i = 0; i < message.payload.size(); i++) {
                    output << std::hex << std::uppercase << std::setfill('0') << std::setw(2)
                           << static_cast<int>(message.payload[i]);
                }
                output << std::endl;
            }
        }
    }
};

int main() {
    ISOTPParser parser;
    std::ifstream input_file("../transcript.txt");
    std::ofstream output_file("../output.txt");
    
    if (!input_file.is_open()) {
        std::cerr << "Error: Could not open transcript.txt" << std::endl;
        return 1;
    }
    
    if (!output_file.is_open()) {
        std::cerr << "Error: Could not create output.txt" << std::endl;
        return 1;
    }
    
    std::string line;
    while (std::getline(input_file, line)) {
        // Remove any whitespace
        line.erase(std::remove_if(line.begin(), line.end(), ::isspace), line.end());
        parser.processFrame(line);
    }
    
    input_file.close();
    
    // Print results to both console and file
    std::cout << "Assembled CAN messages:" << std::endl;
    parser.printCompletedMessages(std::cout);
    
    parser.printCompletedMessages(output_file);
    output_file.close();
    
    std::cout << "\nResults saved to output.txt" << std::endl;
    
    return 0;
}
