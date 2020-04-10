#include <iostream>
#include <fstream>
#include "tclap/CmdLine.h"
#define cimg_use_png
#define cimg_display 0
#include "CImg.h"

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

int main(int argc, char *argv[])
{
    TCLAP::CmdLine cmd("HRPT Decoder by Aang23", ' ', "1.0");
    TCLAP::ValueArg<int> valueChannel("c", "channel", "Channel to extract", true, 0, "int");
    TCLAP::ValueArg<std::string> valueInput("i", "input", "Raw input file", true, "", "string");
    TCLAP::ValueArg<std::string> valueOutput("o", "output", "Output image file", true, "", "string");
    cmd.add(valueChannel);
    cmd.add(valueInput);
    cmd.add(valueOutput);
    try
    {
        cmd.parse(argc, argv);
    }
    catch (TCLAP::ArgException &e)
    {
        std::cout << e.error() << '\n';
        return 0;
    }


    std::cout << valueChannel.getValue() << std::endl;

    std::cout << "Opening input file..." << '\n';
    std::ifstream input_file(valueInput.getValue());

    int total_frame_count = 0;

    long first_frame_pos = -1;

    // Frame sync detection
    uint8_t ch[2];
    uint16_t data;
    int i = 0;
    while (input_file.read((char *)ch, sizeof(ch)))
    {
        data = (ch[1] << 8) | ch[0];

        if (data == HRPT_SYNC[i])
            i++;
        else
            i = 0;

        if (i == HRPT_SYNC_SIZE)
        {
            total_frame_count++;
            i = 0;
            if (first_frame_pos == -1)
                first_frame_pos = (long)input_file.tellg() - 12;
        }
    }

    std::cout << "First frame at " << first_frame_pos << std::endl;
    std::cout << "Found " << total_frame_count << " frames!" << std::endl;

    uint16_t line_buffer[HRPT_SCAN_SIZE];
    int channel = valueChannel.getValue();
    input_file.clear();
    unsigned short imageBuffer[total_frame_count][HRPT_SCAN_WIDTH];

    std::cout << "Decoding channel " << channel << std::endl;

    for (int frame = 0; frame < total_frame_count; frame++)
    {
        long linePos = first_frame_pos + (HRPT_IMAGE_START + frame * HRPT_BLOCK_SIZE) * 2;
        input_file.seekg(linePos);
        input_file.read((char *)line_buffer, HRPT_SCAN_SIZE * 2);

        for (int pixel_pos = 0; pixel_pos < HRPT_SCAN_WIDTH; pixel_pos++)
        {
            uint16_t pixel, pos;
            pos = channel - 1 + pixel_pos * HRPT_NUM_CHANNELS;
            pixel = line_buffer[pos] & 0x03ff;
            imageBuffer[frame][pixel_pos] = pixel * 40;
        }
    }

    cimg_library::CImg<unsigned short> img((unsigned short *)&imageBuffer[0][0], HRPT_SCAN_WIDTH, total_frame_count);
    img.save_png(valueOutput.getValue().c_str());

    std::cout << "Wrote output to "
              << valueOutput.getValue() << std::endl;

    input_file.close();
}
