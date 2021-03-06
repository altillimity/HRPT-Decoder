#include "metop.h"
#include <iostream>
#include "CCSDS/CCSDSSpacePacket.hh"

// HRPT channel count
const int HRPT_NUM_CHANNELS = 5;
// Single image scan word size
const int HRPT_SCAN_WIDTH = 2048;
// Total word size from all channels
const int HRPT_SCAN_SIZE = HRPT_SCAN_WIDTH * HRPT_NUM_CHANNELS;

// Definitely still needs tuning
#define TRANSPORT_THRESOLD_STATE_3 12
#define TRANSPORT_THRESOLD_STATE_2 6
#define TRANSPORT_THRESOLD_STATE_1 2
#define TRANSPORT_THRESOLD_STATE_0 0

// Sync marker word size
const int HRPT_SYNC_SIZE = 4;
// Sync marker
static const uint8_t HRPT_SYNC[HRPT_SYNC_SIZE] = {0x1A, 0xCF, 0xFC, 0x1D};
static const uint32_t HRPT_SYNC_BITS = HRPT_SYNC[0] << 24 | HRPT_SYNC[1] << 16 | HRPT_SYNC[2] << 8 | HRPT_SYNC[3];

// Constructor
METOPDecoder::METOPDecoder(std::ifstream &input) : input_file{input}
{
    // From gr-poes-weather (including comments!)
    unsigned char feedbk, randm = 0xff;
    // Original Polynomial is :  1 + x3 + x5 + x7 +x8
    d_rantab[0] = 0;
    d_rantab[1] = 0;
    d_rantab[2] = 0;
    d_rantab[3] = 0;
    for (int i = 4; i < 1024; i++)
    { //4ASM bytes + 1020bytes = 32 + 8160 bits in CADU packet

        d_rantab[i] = 0;
        for (int j = 0; j <= 7; j++)
        {
            d_rantab[i] = d_rantab[i] << 1;
            if (randm & 0x80) //80h = 1000 0000b
                d_rantab[i]++;

            //Bit-Wise AND between: Fixed shift register(95h) and the state of the
            // feedback register: randm
            feedbk = randm & 0x95; //95h = 1001 0101--> bits 1,3,5,8
            //feedback contains the contents of the registers masked by the polynomial
            //  1 + x3 + x5 + xt +x8 = 95 h
            randm = randm << 1;

            if ((((feedbk & 0x80) ^ (0x80 & feedbk << 3)) ^ (0x80 & (feedbk << 5))) ^ (0x80 & (feedbk << 7)))
                randm++;
        }
    }
}

// Returns the asked bit!
template <typename T>
inline bool getBit(T data, int bit)
{
    return (data >> bit) & 1;
}

// Compare 2 32-bits values bit per bit
int checkSyncMarker2(uint32_t marker, uint32_t totest)
{
    int errors = 0;
    for (int i = 31; i >= 0; i--)
    {
        bool markerBit, testBit;
        markerBit = getBit<uint32_t>(marker, i);
        testBit = getBit<uint32_t>(totest, i);
        if (markerBit != testBit)
            errors++;
    }
    return errors;
}

// Takes 8 bits from a vector and makes a byte
uint8_t convertBitsToByteAtPos2(std::vector<bool> &bitvector, long pos)
{
    uint8_t value = bitvector[pos] << 7 |
                    bitvector[pos + 1] << 6 |
                    bitvector[pos + 2] << 5 |
                    bitvector[pos + 3] << 4 |
                    bitvector[pos + 4] << 3 |
                    bitvector[pos + 5] << 2 |
                    bitvector[pos + 6] << 1 |
                    bitvector[pos + 7];
    return value;
}

// Function doing all the pre-frame work, that is, everything you'd need to do before being ready to read an image
void METOPDecoder::processHRPT()
{
    std::cout << "Reading file..." << '\n';
    // Here we load the entire file into RAM... Should be fine!
    uint8_t bufferByte;
    std::vector<bool> fileContentBin;
    while (input_file.get((char &)bufferByte))
    {
        for (int i = 7; i >= 0; i--)
        {
            bool value = getBit<uint8_t>(bufferByte, i);
            fileContentBin.push_back(!value); // Bit inversion
        }
    }
    input_file.close();

    std::cout << "Detecting synchronization markers..." << '\n';

    // A nice sync machine just like METEOR! 
    int frame_count = 0;
    int thresold_state = 0;
    std::vector<long> frame_starts;
    {
        uint32_t bitBuffer;
        int bitsToIncrement = 1;
        int errors = 0;
        int sep_errors = 0;
        int good = 0;
        int state_2_bits_count = 0;
        int last_state = 0;
        std::cout << "NO LOCK" << std::flush;
        for (long bitPos = 0; bitPos < fileContentBin.size() - 32; bitPos += bitsToIncrement)
        {
            // Well... Easier to work with a 32-bit value here... Needs to be build from the ground up.
            bitBuffer = fileContentBin[bitPos] << 31 |
                        fileContentBin[bitPos + 1] << 30 |
                        fileContentBin[bitPos + 2] << 29 |
                        fileContentBin[bitPos + 3] << 28 |
                        fileContentBin[bitPos + 4] << 27 |
                        fileContentBin[bitPos + 5] << 26 |
                        fileContentBin[bitPos + 6] << 25 |
                        fileContentBin[bitPos + 7] << 24 |
                        fileContentBin[bitPos + 8] << 23 |
                        fileContentBin[bitPos + 9] << 22 |
                        fileContentBin[bitPos + 10] << 21 |
                        fileContentBin[bitPos + 11] << 20 |
                        fileContentBin[bitPos + 12] << 19 |
                        fileContentBin[bitPos + 13] << 18 |
                        fileContentBin[bitPos + 14] << 17 |
                        fileContentBin[bitPos + 15] << 16 |
                        fileContentBin[bitPos + 16] << 15 |
                        fileContentBin[bitPos + 17] << 14 |
                        fileContentBin[bitPos + 18] << 13 |
                        fileContentBin[bitPos + 19] << 12 |
                        fileContentBin[bitPos + 20] << 11 |
                        fileContentBin[bitPos + 21] << 10 |
                        fileContentBin[bitPos + 22] << 9 |
                        fileContentBin[bitPos + 23] << 8 |
                        fileContentBin[bitPos + 24] << 7 |
                        fileContentBin[bitPos + 25] << 6 |
                        fileContentBin[bitPos + 26] << 5 |
                        fileContentBin[bitPos + 27] << 4 |
                        fileContentBin[bitPos + 28] << 3 |
                        fileContentBin[bitPos + 29] << 2 |
                        fileContentBin[bitPos + 30] << 1 |
                        fileContentBin[bitPos + 31];

            // State 0 : Searches bit-per-bit for a perfect sync marker. If one is found, we jump to state 6!
            if (thresold_state == TRANSPORT_THRESOLD_STATE_0)
            {
                if (checkSyncMarker2(HRPT_SYNC_BITS, bitBuffer) <= thresold_state)
                {
                    frame_count++;
                    frame_starts.push_back(bitPos);
                    thresold_state = TRANSPORT_THRESOLD_STATE_1;
                    bitsToIncrement = 1024 * 8;
                    errors = 0;
                    sep_errors = 0;
                    good = 0;
                }
            }
            // State 1 : Each header is expect 1024 bytes away. Only 6 mistmatches tolerated.
            // If 5 consecutive good frames are found, we hop to state 22, though, 5 consecutive
            // errors (here's why errors is reset each time a frame is good) means reset to state 0
            // 2 frame errors pushes us to state 2
            else if (thresold_state == TRANSPORT_THRESOLD_STATE_1)
            {
                if (checkSyncMarker2(HRPT_SYNC_BITS, bitBuffer) <= thresold_state)
                {
                    frame_count++;
                    frame_starts.push_back(bitPos);
                    good++;
                    errors = 0;

                    if (good == 5)
                    {
                        thresold_state = TRANSPORT_THRESOLD_STATE_3;
                        good = 0;
                        errors = 0;
                    }
                }
                else
                {
                    errors++;
                    sep_errors++;

                    if (errors == 5)
                    {
                        thresold_state = TRANSPORT_THRESOLD_STATE_0;
                        bitsToIncrement = 1;
                        errors = 0;
                        sep_errors = 0;
                        good = 0;
                    }

                    if (sep_errors == 2)
                    {
                        thresold_state = TRANSPORT_THRESOLD_STATE_2;
                        state_2_bits_count = 0;
                        bitsToIncrement = 1;
                        errors = 0;
                        sep_errors = 0;
                        good = 0;
                    }
                }
            }
            // State 2 : Goes back to bit-per-bit syncing... 3 frame scanned and we got back to state 0, 1 good and back to 6!
            else if (thresold_state == TRANSPORT_THRESOLD_STATE_2)
            {
                if (checkSyncMarker2(HRPT_SYNC_BITS, bitBuffer) <= thresold_state)
                {
                    frame_count++;
                    frame_starts.push_back(bitPos);
                    thresold_state = TRANSPORT_THRESOLD_STATE_1;
                    bitsToIncrement = 1024 * 8;
                    errors = 0;
                    sep_errors = 0;
                    good = 0;
                }
                else
                {
                    state_2_bits_count++;
                    errors++;

                    if (state_2_bits_count >= 3072 * 8)
                    {
                        thresold_state = TRANSPORT_THRESOLD_STATE_0;
                        bitsToIncrement = 1;
                        errors = 0;
                        sep_errors = 0;
                        good = 0;
                    }
                }
            }
            // State 3 : We assume perfect lock and allow very high mismatchs.
            // 1 error and back to state 6
            // Note : Lowering the thresold seems to yield better of a sync
            else if (thresold_state == TRANSPORT_THRESOLD_STATE_3)
            {
                if (checkSyncMarker2(HRPT_SYNC_BITS, bitBuffer) <= thresold_state)
                {
                    frame_count++;
                    frame_starts.push_back(bitPos);
                }
                else
                {
                    errors = 0;
                    good = 0;
                    sep_errors = 0;
                    thresold_state = TRANSPORT_THRESOLD_STATE_1;
                }
            }

            if (last_state != thresold_state)
            {
                std::cout << (thresold_state > 0 ? "\rLOCKED " : "\rNO LOCK") << std::flush;
                last_state = thresold_state;
            }
        }
    }
    std::cout << '\n';
    std::cout << "Done! Found " << frame_count << " sync markers!" << '\n';

    std::cout << "Processing VCDUs and CCSDS frames..." << '\n';

    std::ofstream output_file = std::ofstream("temp.ccsds", std::ios::binary);

    // We need to know where our frames are
    std::vector<long> ccsdsFrameStarts;
    // Count of VCID 9 frame founds
    int count9 = 0;
    for (long current_frame_pos : frame_starts)
    {
        std::vector<uint8_t> packetVec;

        // Read everything but the header
        for (int u = 4; u < 1024; u++)
        {
            uint8_t dataByte = convertBitsToByteAtPos2(fileContentBin, current_frame_pos + u * 8);
            dataByte = dataByte ^ d_rantab[u]; // Derandomize
            packetVec.push_back(dataByte);
        }

        int vcid = (packetVec[1] % 64); // Extract VCID from header

        // Select only AHRR data
        if (vcid == 9)
        {
            // Read M-PDU
            int mpdu_spare = (packetVec[8] >> 3);
            int mpdu_header = ((packetVec[8] % 8) << 8) | packetVec[9];
            
            // Write data into our buffer file
            for (int i = 0; i < 882; i++)
                output_file.put((char &)packetVec[10 + i]);

            // If there is a header, save its position
            if (mpdu_spare == 0)
                ccsdsFrameStarts.push_back(count9 * 882 + mpdu_header);

            count9++;
        }
    }

    std::cout << "Found " << count9 << " VCDUs with VCID 9" << '\n';
    std::cout << "Found " << ccsdsFrameStarts.size() << " CCDSDS frames headers declared" << '\n';
    output_file.close();

    input_file = std::ifstream("temp.ccsds", std::ios::binary);

    // Now reading CCSDS frames found earlier
    for (int frame_num = 0; frame_num < ccsdsFrameStarts.size(); frame_num++)
    {
        long frame_start = ccsdsFrameStarts[frame_num];
        // Compute frame size
        int frame_size = ccsdsFrameStarts[frame_num + 1] - frame_start;

        // I guess there's no data if the frame has such a small length?
        if (frame_size <= 0)
            break;

        // Buffer for CCSDS packet
        std::vector<uint8_t> ccsds_packet;
        input_file.seekg(frame_start);

        // Fill our buffer
        for (int i = 0; i < frame_size; i++)
        {
            char ch;
            input_file.get((char &)ch);
            ccsds_packet.push_back(ch);
        }

        // Parse the packet! This library makes it much easier...
        CCSDSSpacePacket truePacket;
        try
        {
            truePacket.interpret(ccsds_packet);
        }
        catch (CCSDSSpacePacketException e)
        {
            continue; // Exit on error, we do not want to read corrupted frames since that's undefined behavior.
        }

        // APID Checking!
        int APID = truePacket.getPrimaryHeader()->getAPIDAsInteger();

        // Only work on APID 103 and 104. Allowing both
        if (APID == 103 | APID == 104)
        {
            total_frame_count++;

            // We want the payload... So here we go!
            std::vector<uint8_t> *userData = truePacket.getUserDataField();
            // Buffer for the current AVHRR scanline
            std::array<uint16_t, 10240> line_buffer;

            // Apparently data is sometime shifted, what is indicated by the first byte's value...
            // Probably doing it wrong? But it works... Needs finer tuning
            int pos = (*userData)[0] > 20 ? 80 : 58;

            // Read a scanline. 10-bits values again, may need a rewrite...
            for (int i = 0; i < HRPT_SCAN_SIZE; i += 4)
            {
                uint16_t pixel1, pixel2, pixel3, pixel4;
                pixel1 = ((*userData)[pos + 0] << 2) | ((*userData)[pos + 1] >> 6);
                pixel2 = (((*userData)[pos + 1] % 64) << 4) | ((*userData)[pos + 2] >> 4);
                pixel3 = (((*userData)[pos + 2] % 16) << 6) | ((*userData)[pos + 3] >> 2);
                pixel4 = (((*userData)[pos + 3] % 4) << 8) | (*userData)[pos + 4];
                pos += 5;

                line_buffer[i] = pixel1;
                line_buffer[i + 1] = pixel2;
                line_buffer[i + 2] = pixel3;
                line_buffer[i + 3] = pixel4;
            }

            scanLines.push_back(line_buffer);
        }
    }
    output_file.close();

    std::cout << total_frame_count << " CCSDS frames of APID 103 or 104" << '\n';
}

// Function used to decode a choosen channel
cimg_library::CImg<unsigned short> METOPDecoder::decodeChannel(int channel)
{
    // Create a buffer for an entire line
    std::array<uint16_t, 10240> line_buffer;
    // Reset our ifstream
    input_file.clear();

    // Large passes are too good for the stack to take it...
    unsigned short *imageBuffer = new unsigned short[total_frame_count * HRPT_SCAN_WIDTH];

    // Loop through all frames
    for (int frame = 0; frame < total_frame_count; frame++)
    {
        line_buffer = scanLines[frame];

        // Loop through all pixels of the current line
        for (int pixel_pos = 0; pixel_pos < HRPT_SCAN_WIDTH; pixel_pos++)
        {
            // Compute pixel position, read, and put into the image buffer
            uint16_t pixel, pos;
            pos = channel - 1 + pixel_pos * HRPT_NUM_CHANNELS;
            pixel = line_buffer[pos];
            imageBuffer[frame * HRPT_SCAN_WIDTH + pixel_pos] = pixel * 60;
        }
    }

    // Build an image and return it
    cimg_library::CImg<unsigned short> channelImage(&imageBuffer[0], HRPT_SCAN_WIDTH, total_frame_count);
    delete[] imageBuffer; // Don't forget to free up the heap!

    return channelImage;
}

// Return total fram count
int METOPDecoder::getTotalFrameCount()
{
    return total_frame_count;
}

// Perform a cleanup..
void METOPDecoder::cleanupFiles()
{
    // Goodbye CCSDS!
    std::remove("temp.ccsds");
}
