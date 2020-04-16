#include "meteor.h"
#include "manchester.h"
#include <iostream>

// Total world count
const int HRPT_TRANSPORT_SIZE = 1024;
// Channel count
const int HRPT_NUM_CHANNELS = 5;
// Single image scan word size
const int HRPT_SCAN_WIDTH = 1572;
// Total word size from all channels
const int HRPT_SCAN_SIZE = HRPT_SCAN_WIDTH * HRPT_NUM_CHANNELS;
// Image words position from frame sync
const int HRPT_IMAGE_START = 750;
// Sync marker word size
const int HRPT_SYNC_SIZE = 6;
// Sync marker
static const uint16_t HRPT_SYNC[HRPT_SYNC_SIZE] = {0x0284, 0x016F, 0x035C, 0x019D, 0x020F, 0x0095};

// Constructor
METEORDecoder::METEORDecoder(std::ifstream &input) : input_file{input}
{
}

void bin(uint32_t n)
{
    uint32_t i;
    for (i = 1 << 31; i > 0; i = i / 2)
        (n & i) ? printf("1") : printf("0");
}

// Returns the asked bit!
template <typename T>
inline bool getBit(T data, int bit)
{
    return (data >> bit) & 1;
}

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
    uint8_t ch[2];
    std::cout << "Performing Manchester decoding... " << '\n';
    std::ofstream output_file("temp.man", std::ios::binary);
    while (input_file.read((char *)ch, sizeof(ch)))
        output_file.put((char)manchester_decode(ch[1], ch[0]));
    std::cout << "Done!" << '\n';
    output_file.close();
    input_file.close();

    uint8_t ch2;
    int i = 0;

    // Transport frame sync. Implementation of http://www.sat.cc.ua/data/CADU%20Frame%20Synchro.pdf
    const int HRPT_SYNC_SIZE = 4;
    static const uint8_t HRPT_SYNC[HRPT_SYNC_SIZE] = {0x1A, 0xCF, 0xFC, 0x1D};

    uint32_t HRPT_SYNC_BITS = HRPT_SYNC[0] << 24 | HRPT_SYNC[1] << 16 | HRPT_SYNC[2] << 8 | HRPT_SYNC[3];

    input_file = std::ifstream("temp.man");

    int frame_count = 0;
    int thresold_state = 0;
    std::vector<long> frame_starts;

    std::cout << "Searching for synchronization markers..." << '\n';

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

    uint32_t bitBuffer;
    int bitsToIncrement = 1;
    int errors = 0;
    int sep_errors;
    int good = 0;
    int state_2_bits_count;
    int last_state = 0;
    for (long bitPos = 0; bitPos < fileContentBin.size() - 32; bitPos += bitsToIncrement)
    {
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
        /*if(last_state != thresold_state) {
            std::cout << thresold_state << std::endl;
            last_state = thresold_state;
        }*/
    }

    std::cout << "Found " << frame_count << " valid sync markers!" << '\n';

    // Demultiplexing
    std::cout << "Demultiplexing MSU-MR..." << '\n';
    output_file = std::ofstream("temp.msumr", std::ios::binary);
    input_file.clear();

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

    output_file.close();
    input_file.close();

    // MSU-MR Sync
    input_file.open("temp.msumr");

    const int HRPT_MSU_SYNC_SIZE = 8;
    static const uint8_t HRPT_MSU_SYNC[HRPT_MSU_SYNC_SIZE] = {2, 24, 167, 163, 146, 221, 154, 191};

    i = 0;

    long msu_file_byte_size = 0;
    while (input_file.get((char &)ch2))
    {
        if (ch2 == HRPT_MSU_SYNC[i])
            i++;
        else
            i = 0;

        // If all 6 matched, we got a frame!
        if (i == HRPT_MSU_SYNC_SIZE)
        {
            //std::cout << "FRAME" << '\n';
            // Reset i for next frame
            i = 0;
            total_mru_frame_count++;
            msu_frame_starts.push_back((long)input_file.tellg() - 8);
            if (mru_first_frame_pos == -1)
                mru_first_frame_pos = (long)input_file.tellg() - 8;
        }
        msu_file_byte_size++;
    }

    /*for (int k = 0; k < msu_frame_starts.size(); k++)
            std::cout << msu_frame_starts[k + 1] - msu_frame_starts[k] << std::endl;*/

    std::cout << "Found " << total_mru_frame_count << " valid MSU-MR sync markers!" << '\n';
}

// Function used to decode a choosen channel
cimg_library::CImg<unsigned short> METEORDecoder::decodeChannel(int channel)
{
    input_file.clear();

    unsigned short *imageBuffer = new unsigned short[total_mru_frame_count * HRPT_SCAN_WIDTH];

    long linecount = 0;
    for (/*int msu_frame_nm = 0; msu_frame_nm < ((msu_file_byte_size - mru_first_frame_pos) / 11850); msu_frame_nm++*/ long frame_pos : msu_frame_starts)
    {
        //long frame_pos = mru_first_frame_pos + (11850 * msu_frame_nm);
        uint8_t msumr_frame_buffer[11850];
        input_file.seekg(frame_pos);
        input_file.read((char *)msumr_frame_buffer, sizeof(msumr_frame_buffer));

        uint16_t line_buffer[1572];

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

            uint16_t pixel1, pixel2, pixel3, pixel4;
            pixel1 = (pixel_buffer_4[0] << 2) | (pixel_buffer_4[1] >> 6);
            pixel2 = ((pixel_buffer_4[1] % 64) << 4) | (pixel_buffer_4[2] >> 4);
            pixel3 = ((pixel_buffer_4[2] % 16) << 6) | (pixel_buffer_4[3] >> 2);
            pixel4 = ((pixel_buffer_4[3] % 4) << 8) | pixel_buffer_4[4];

            int currentImagePixelPos = l * 4;
            imageBuffer[linecount * 1572 + currentImagePixelPos] = pixel1 * 60;
            imageBuffer[linecount * 1572 + currentImagePixelPos + 1] = pixel2 * 60;
            imageBuffer[linecount * 1572 + currentImagePixelPos + 2] = pixel3 * 60;
            imageBuffer[linecount * 1572 + currentImagePixelPos + 3] = pixel4 * 60;
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
