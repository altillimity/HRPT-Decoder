#include "metop.h"
#include <iostream>
#include "CCSDS/CCSDSSpacePacket.hh"

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

void bin(uint8_t n)
{
    uint8_t i;
    for (i = (uint8_t)1 << 7; i > 0; i = i / 2)
        (n & i) ? printf("1") : printf("0");
}

void bin16(uint16_t n)
{
    uint16_t i;
    for (i = (uint16_t)1 << 15; i > 0; i = i / 2)
        (n & i) ? printf("1") : printf("0");
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
    std::cout << "Detecting synchronization markers..." << '\n';

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

    std::cout << fileContentBin.size() << " bits" << std::endl;

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

    std::ofstream output_file = std::ofstream("temp.ccsds", std::ios::binary);

    std::vector<long> ccsdsFrameStarts;
    long totalbyteCount = 0;
    int count9 = 0;
    for (long current_frame_pos : frame_starts)
    {
        std::vector<uint8_t> packetVec;

        for (int u = 4; u < 1024; u++)
        {
            uint8_t dataByte = convertBitsToByteAtPos2(fileContentBin, current_frame_pos + u * 8);
            dataByte = dataByte ^ d_rantab[u];

            //output_file.put((char &)dataByte);
            packetVec.push_back(dataByte);
        }

        //int version = packetVec[0] >> 6;
        //int scid = ((packetVec[0] % 64) << 2) | (packetVec[1] >> 6);
        int vcid = (packetVec[1] % 64);

        if (vcid == 9)
        {
            int mpdu_spare = (packetVec[8] >> 3);
            int mpdu_header = ((packetVec[8] % 8) << 8) | packetVec[9];
            //bin(mpdu_spare);
            //std::cout << " " << mpdu_spare <<'\n';
            //bin16(mpdu_header);
            //std::cout << " " << mpdu_header << '\n';

            //output_file.put((char &)HRPT_SYNC[0]);
            //output_file.put((char &)HRPT_SYNC[1]);
            //output_file.put((char &)HRPT_SYNC[2]);
            //output_file.put((char &)HRPT_SYNC[3]);
            for (int i = 0; i < 882; i++)
            {

                output_file.put((char &)packetVec[10 + i]);
            }

            if (mpdu_spare == 0)
            {
                ccsdsFrameStarts.push_back(count9 * 882 + mpdu_header);
                //if(ccsdsFrameStarts[ccsdsFrameStarts.size() - 1] - ccsdsFrameStarts[ccsdsFrameStarts.size() - 2] > 12966);
                //std::cout << "CCSDS frame starting at : " << /*totalbyteCount + mpdu_header << " --- " << ccsdsFrameStarts[ccsdsFrameStarts.size() - 1] - ccsdsFrameStarts[ccsdsFrameStarts.size() - 2]*/mpdu_header << '\n';
            }
            count9++;
        }
    }

    std::cout << "Found " << count9 << " VCDUs with VCID 9" << '\n';
    std::cout << "Found " << ccsdsFrameStarts.size() << " CCDSDS frames headers declared" << '\n';
    output_file.close();
    input_file = std::ifstream("temp.ccsds", std::ios::binary);
    output_file = std::ofstream("temp.avhrr", std::ios::binary);

    for (int frame_num = 0; frame_num < ccsdsFrameStarts.size(); frame_num++)
    {
        long frame_start = ccsdsFrameStarts[frame_num];
        int frame_size = frame_num + 1 == ccsdsFrameStarts.size() ? 0 : ccsdsFrameStarts[frame_num + 1] - frame_start;

        if (frame_size <= 0)
            break;

        std::vector<uint8_t> ccsds_packet;
        input_file.seekg(frame_start);

        for (int i = 0; i < frame_size; i++)
        {
            char ch;
            input_file.get((char &)ch);
            ccsds_packet.push_back(ch);
        }

        CCSDSSpacePacket truePacket;
        try
        {
            truePacket.interpret(ccsds_packet);
        }
        catch (CCSDSSpacePacketException e)
        {
            //std::cout << e.toString() << '\n';
            continue;
        }

        int APID = truePacket.getPrimaryHeader()->getAPIDAsInteger(); //((ccsds_packet[0] % 8) << 8) | ccsds_packet[1];

        if (APID == 103 | APID == 104)
        {
            total_frame_count++;

            //std::cout << truePacket.toString() << '\n';

            std::vector<uint8_t> *userData = truePacket.getUserDataField();

            output_file.put((char &)HRPT_SYNC[0]);
            output_file.put((char &)HRPT_SYNC[1]);
            output_file.put((char &)HRPT_SYNC[2]);
            output_file.put((char &)HRPT_SYNC[3]);

            for (uint8_t byte_temp : *userData)
                output_file.put(byte_temp);

            std::array<uint16_t, 10240> line_buffer;

            int pos = (*userData)[0] == 158 /*MetOp-C*/ | (*userData)[0] == 71 /*MetOp-B*/ | (*userData)[0] == 38 /*MetOp-A*/ ? 80 : 58;

            std::cout << (int) (*userData)[0] << '\n';

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

    std::cout << total_frame_count << " CCSDS frames of APID 103" << '\n';
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
