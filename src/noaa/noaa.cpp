#include "noaa.h"

// Total world count
const int HRPT_BLOCK_SIZE = 11090;
// Channel count
const int HRPT_NUM_CHANNELS = 5;
// Single image scan word size
const int HRPT_SCAN_WIDTH = 2048;
// Total word size from all channels
const int HRPT_SCAN_SIZE = HRPT_SCAN_WIDTH * HRPT_NUM_CHANNELS;
// Image words position from frame sync
const int HRPT_IMAGE_START = 750;
// Sync marker word size
const int HRPT_SYNC_SIZE = 6;
// Sync marker
static const uint16_t HRPT_SYNC[HRPT_SYNC_SIZE] = {0x0284, 0x016F, 0x035C, 0x019D, 0x020F, 0x0095};

// Constructor
NOAADecoder::NOAADecoder(std::ifstream &input) : input_file{input}
{
}

// Function doing all the pre-frame work, that is, everything you'd need to do before being ready to read an image
void NOAADecoder::processHRPT()
{
    // Frame sync detection... Perfect markers everywhere so easy enough!
    uint8_t ch[2];
    uint16_t data;
    int i = 0;
    while (input_file.read((char *)ch, sizeof(ch)))
    {
        // Little-Endian encoding
        data = (ch[1] << 8) | ch[0];

        // Check sync frame, if it matches, increment and check the next one
        if (data == HRPT_SYNC[i])
            i++;
        else
            i = 0;

        // If all 6 matched, we got a frame!
        if (i == HRPT_SYNC_SIZE)
        {
            total_frame_count++;
            // Reset i for next frame
            i = 0;
            // First one detected, save it
            if (first_frame_pos == -1)
                first_frame_pos = (long)input_file.tellg() - 12;
        }
    }
}

// Function used to decode a choosen channel
cimg_library::CImg<unsigned short> NOAADecoder::decodeChannel(int channel)
{
    // Create a buffer for an entire line
    uint16_t line_buffer[HRPT_SCAN_SIZE];
    // Reset our ifstream
    input_file.clear();

    // Large passes are too good for the stack to take it...
    unsigned short *imageBuffer = new unsigned short[total_frame_count * HRPT_SCAN_WIDTH];

    // Loop through all frames
    for (int frame = 0; frame < total_frame_count; frame++)
    {
        // Read a line from the current frame (AVHRR data)
        long linePos = first_frame_pos + (HRPT_IMAGE_START + frame * HRPT_BLOCK_SIZE) * 2;
        input_file.seekg(linePos);
        input_file.read((char *)line_buffer, HRPT_SCAN_SIZE * 2);

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
int NOAADecoder::getTotalFrameCount()
{
    return total_frame_count;
}
