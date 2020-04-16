#include "meteor.h"
#include "manchester.h"
#include <iostream>
#include <cstdio>

// Total world count
const int HRPT_TRANSPORT_SIZE = 1024;
// Channel count
const int HRPT_NUM_CHANNELS = 6;
// Single image scan word size
const int HRPT_SCAN_WIDTH = 1572;
// Sync marker word size
const int HRPT_SYNC_SIZE = 4;
// Sync marker
static const uint8_t HRPT_SYNC[HRPT_SYNC_SIZE] = {0x1A, 0xCF, 0xFC, 0x1D};
static const uint32_t HRPT_SYNC_BITS = HRPT_SYNC[0] << 24 | HRPT_SYNC[1] << 16 | HRPT_SYNC[2] << 8 | HRPT_SYNC[3];
// MSU-MR Sync marker
const int HRPT_SYNC_SIZE_MSU_MR = 8;
static const uint16_t HRPT_SYNC_MSU_MR[HRPT_SYNC_SIZE_MSU_MR] = {0x0284, 0x016F, 0x035C, 0x019D, 0x020F, 0x0095};

// Constructor
METEORDecoder::METEORDecoder(std::ifstream &input) : input_file{input}
{
}

// Returns the asked bit!
template <typename T>
inline bool getBit(T data, int bit)
{
    return (data >> bit) & 1;
}

// Compare 2 32-bits values bit per bit
int checkSyncMarker(uint32_t marker, uint32_t totest)
{
    int errors = 0;
    for (int i = 31; i >= 0; i--)
    {
        bool markerBit, testBit;
        markerBit = getBit(marker, i);
        testBit = getBit(totest, i);
        if (markerBit != testBit)
            errors++;
    }
    return errors;
}

// Takes 8 bits from a vector and makes a byte
uint8_t convertBitsToByteAtPos(std::vector<bool> &bitvector, long pos)
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
void METEORDecoder::processHRPT()
{
    // Manchester decoding
    std::cout << "Performing Manchester decoding... " << '\n';
    std::ofstream output_file("temp.man", std::ios::binary); // Open our output file
    uint8_t ch[2];                                           // We need to read 2 bytes at once
    while (input_file.read((char *)ch, sizeof(ch)))
        output_file.put((char)manchester_decode(ch[1], ch[0]));
    std::cout << "Done!" << '\n';
    output_file.close();
    input_file.close();

    // Transport frame sync.
    input_file = std::ifstream("temp.man", std::ios::binary); // Now we'e working Manchester-free

    // Multiple variables we'll need
    int frame_count = 0;
    int thresold_state = 0;
    std::vector<long> frame_starts;

    std::cout << "Reading file..." << '\n';

    // This may seem dumb at first, but considering decoded files are not several gigabytes... Working with a
    // vector eases the process a lot, and using RAM is also supposedly faster?
    // So here I'm read the entire file bit-per-bit into a vector of bools!
    uint8_t bufferByte;
    std::vector<bool> fileContentBin;
    while (input_file.get((char &)bufferByte))
    {
        for (int i = 7; i >= 0; i--)
        {
            bool value = getBit<uint8_t>(bufferByte, i);
            fileContentBin.push_back(value);
        }
    }

    std::cout << "Searching for synchronization markers..." << '\n';

    // Searching for sync markers... Implementation of http://www.sat.cc.ua/data/CADU%20Frame%20Synchro.pdf
    // NOTE : Needs tuning? Is the implementation perfect? (Slight changes yield more frames)
    uint32_t bitBuffer;
    int bitsToIncrement = 1;
    int errors = 0;
    int sep_errors = 0;
    int good = 0;
    int state_2_bits_count = 0;
    int last_state = 0;
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

        // State 0 : Searched bit-per-bit for a perfect sync marker. If one is found, we jump to state 6!
        if (thresold_state == 0)
        {
            if (checkSyncMarker(HRPT_SYNC_BITS, bitBuffer) <= thresold_state)
            {
                frame_count++;
                frame_starts.push_back(bitPos);
                thresold_state = 6;
                bitsToIncrement = 1024 * 8;
                errors = 0;
                sep_errors = 0;
                good = 0;
            }
        }
        // State 6 : Each header is expect 1024 bytes away. Only 6 mistmatches tolerated.
        // If 5 consecutive good frames are found, we hop to state 22, though, 5 consecutive
        // errors (here's why errors is reset each time a frame is good) means reset to state 0
        // 2 frame errors pushes us to state 2
        else if (thresold_state == 6)
        {
            if (checkSyncMarker(HRPT_SYNC_BITS, bitBuffer) <= thresold_state)
            {
                frame_count++;
                frame_starts.push_back(bitPos);
                good++;
                errors = 0;

                if (good == 5)
                {
                    thresold_state = 22;
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
                    thresold_state = 0;
                    bitsToIncrement = 1;
                    errors = 0;
                    sep_errors = 0;
                    good = 0;
                }

                if (sep_errors == 2)
                {
                    thresold_state = 2;
                    state_2_bits_count = 0;
                    bitsToIncrement = 1;
                    errors = 0;
                    sep_errors = 0;
                    good = 0;
                }
            }
        }
        // State 2 : Goes back to bit-per-bit syncing... 3 frame scanned and we got back to state 0, 1 good and back to 6!
        else if (thresold_state == 2)
        {
            if (checkSyncMarker(HRPT_SYNC_BITS, bitBuffer) <= thresold_state)
            {
                frame_count++;
                frame_starts.push_back(bitPos);
                thresold_state = 6;
                bitsToIncrement = 1024 * 8;
                errors = 0;
                sep_errors = 0;
                good = 0;
            }
            else
            {
                state_2_bits_count++;
                errors++;

                if (state_2_bits_count >= 3072 * 8 /*&& errors <= 5*/)
                {
                    thresold_state = 0;
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
        else if (thresold_state == 22)
        {
            if (checkSyncMarker(HRPT_SYNC_BITS, bitBuffer) <= thresold_state)
            {
                frame_count++;
                frame_starts.push_back(bitPos);
            }
            else
            {
                errors = 0;
                good = 0;
                sep_errors = 0;
                thresold_state = 6;
            }
        }
    }

    std::cout << "Found " << frame_count << " valid sync markers!" << '\n';

    // Demultiplexing
    std::cout << "Demultiplexing MSU-MR..." << '\n';
    output_file = std::ofstream("temp.msumr", std::ios::binary);
    input_file.clear();

    // Extracing MSU-MR data! Since we probably aren't in sync with byte spacing...
    // Still bit-per-bit and based on our frame starts saved earlier
    for (long bitPos : frame_starts)
    {
        int msu_mr_data_pos = bitPos + 22 * 8;
        for (int byteReadPos = 0; byteReadPos < 238; byteReadPos++)
        {
            uint8_t byte = convertBitsToByteAtPos(fileContentBin, msu_mr_data_pos + byteReadPos * 8);
            output_file.put(byte);
        }

        msu_mr_data_pos = bitPos + 278 * 8;
        for (int byteReadPos = 0; byteReadPos < 238; byteReadPos++)
        {
            uint8_t byte = convertBitsToByteAtPos(fileContentBin, msu_mr_data_pos + byteReadPos * 8);
            output_file.put(byte);
        }

        msu_mr_data_pos = bitPos + 534 * 8;
        for (int byteReadPos = 0; byteReadPos < 238; byteReadPos++)
        {
            uint8_t byte = convertBitsToByteAtPos(fileContentBin, msu_mr_data_pos + byteReadPos * 8);
            output_file.put(byte);
        }

        msu_mr_data_pos = bitPos + 790 * 8;
        for (int byteReadPos = 0; byteReadPos < 234; byteReadPos++)
        {
            uint8_t byte = convertBitsToByteAtPos(fileContentBin, msu_mr_data_pos + byteReadPos * 8);
            output_file.put(byte);
        }
    }

    // Some cleanup...
    output_file.close();
    input_file.close();

    // MSU-MR Sync
    input_file.open("temp.msumr", std::ios::binary);

    int i = 0;
    uint8_t ch2;

    // Here we can check for valid header only...
    // NOTE : Sync machine system? Error thresold?
    while (input_file.get((char &)ch2))
    {
        if (ch2 == HRPT_SYNC_MSU_MR[i])
            i++;
        else
            i = 0;

        // If all 6 matched, we got a frame!
        if (i == HRPT_SYNC_SIZE_MSU_MR)
        {
            // Reset i for next frame
            i = 0;
            total_mru_frame_count++;
            msu_frame_starts.push_back((long)input_file.tellg() - HRPT_SYNC_SIZE_MSU_MR);
            if (mru_first_frame_pos == -1)
                mru_first_frame_pos = (long)input_file.tellg() - HRPT_SYNC_SIZE_MSU_MR;
        }
    }

    std::cout << "Found " << total_mru_frame_count << " valid MSU-MR sync markers!" << '\n';
    input_file.close();
}

// Function used to decode a choosen channel
cimg_library::CImg<unsigned short> METEORDecoder::decodeChannel(int channel)
{
    input_file.clear();

    // Large passes are too good for the stack to take it...
    unsigned short *imageBuffer = new unsigned short[total_mru_frame_count * HRPT_SCAN_WIDTH];

    long linecount = 0;
    for (long frame_pos : msu_frame_starts)
    {
        /// Go at the beggining of the frame
        uint8_t msumr_frame_buffer[11850];
        input_file.seekg(frame_pos);
        input_file.read((char *)msumr_frame_buffer, sizeof(msumr_frame_buffer));

        uint16_t line_buffer[HRPT_SCAN_WIDTH];

        // 393 byte per channel
        for (int l = 0; l < 393; l++)
        {
            uint8_t pixel_buffer_4[5];
            int pixelpos = 50 + l * 30 + (channel - 1) * 5;

            pixel_buffer_4[0] = msumr_frame_buffer[pixelpos];
            pixel_buffer_4[1] = msumr_frame_buffer[pixelpos + 1];
            pixel_buffer_4[2] = msumr_frame_buffer[pixelpos + 2];
            pixel_buffer_4[3] = msumr_frame_buffer[pixelpos + 3];
            pixel_buffer_4[4] = msumr_frame_buffer[pixelpos + 4];

            // Convert 5 bytes to 4 10-bits values
            uint16_t pixel1, pixel2, pixel3, pixel4;
            pixel1 = (pixel_buffer_4[0] << 2) | (pixel_buffer_4[1] >> 6);
            pixel2 = ((pixel_buffer_4[1] % 64) << 4) | (pixel_buffer_4[2] >> 4);
            pixel3 = ((pixel_buffer_4[2] % 16) << 6) | (pixel_buffer_4[3] >> 2);
            pixel4 = ((pixel_buffer_4[3] % 4) << 8) | pixel_buffer_4[4];

            int currentImagePixelPos = l * 4;
            imageBuffer[linecount * HRPT_SCAN_WIDTH + currentImagePixelPos] = pixel1 * 60;
            imageBuffer[linecount * HRPT_SCAN_WIDTH + currentImagePixelPos + 1] = pixel2 * 60;
            imageBuffer[linecount * HRPT_SCAN_WIDTH + currentImagePixelPos + 2] = pixel3 * 60;
            imageBuffer[linecount * HRPT_SCAN_WIDTH + currentImagePixelPos + 3] = pixel4 * 60;
        }
        linecount++;
    }

    // Build an image and return it
    cimg_library::CImg<unsigned short> channelImage(&imageBuffer[0], HRPT_SCAN_WIDTH, total_mru_frame_count);
    delete[] imageBuffer; // Don't forget to free up the heap!
    return channelImage;
}

// Return total frame count
int METEORDecoder::getTotalFrameCount()
{
    return total_mru_frame_count;
}

// Perform a cleanup..
void METEORDecoder::cleanupFiles()
{
    // Goodbye Manchester!
    std::remove("temp.man");
    // All the hard work... Welcome disk space?
    std::remove("temp.msumr");
}
