#include <iostream>
#include <fstream>
#include "tclap/CmdLine.h"
#define cimg_use_png
#define cimg_display 0
#include "CImg.h"

#include "noaa/noaa.h"
#include "meteor/meteor.h"

int main(int argc, char *argv[])
{
    TCLAP::CmdLine cmd("HRPT Decoder by Aang23", ' ', "1.0");

    // Channel or mode selection
    TCLAP::ValueArg<int> valueChannel("c", "channel", "Channel to extract", true, 0, "channel");
    TCLAP::SwitchArg optionFalseColor("f", "falsecolor", "Produce false-color image");

    // IO arguments
    TCLAP::ValueArg<std::string> valueInput("i", "input", "Raw input file", true, "", "file");
    TCLAP::ValueArg<std::string> valueOutput("o", "output", "Output image file", true, "", "image.png");

    // Satellite decoders
    std::vector<std::string> decoders;
    decoders.push_back("NOAA");
    decoders.push_back("METEOR");
    decoders.push_back("MetOp");
    decoders.push_back("FengYun");
    TCLAP::ValuesConstraint<std::string> decodersAllowed(decoders);
    TCLAP::ValueArg<std::string> satelliteArg("t", "type", "Satellite to decode", true, "NOAA", &decodersAllowed);

    // Pass direction
    TCLAP::SwitchArg optionSouthbound("S", "southbound", "Southbound pass (defaults to Northbound)");

    // Other arguments
    TCLAP::ValueArg<int> valueEqualize("e", "equalization", "Equalization to apply", false, 200, "equalization");

    // Register all of the above options
    cmd.add(satelliteArg);
    cmd.add(valueInput);
    cmd.add(valueOutput);
    cmd.xorAdd(valueChannel, optionFalseColor);
    cmd.add(optionSouthbound);
    cmd.add(valueEqualize);

    // Parse
    try
    {
        cmd.parse(argc, argv);
    }
    catch (TCLAP::ArgException &e)
    {
        std::cout << e.error() << '\n';
        return 0;
    }

    // Variable we need watever we do
    std::ifstream input_file(valueInput.getValue(), std::ios::binary);
    cimg_library::CImg<unsigned short> final_image;

    if (satelliteArg.getValue() == "NOAA")
    {
        // NOAA decoding!
        std::cout << "Decoding NOAA!" << std::endl;

        NOAADecoder decoder(input_file);
        decoder.processHRPT();

        final_image = cimg_library::CImg<unsigned short>(2048, decoder.getTotalFrameCount(), 1, 3);

        if (optionFalseColor.getValue())
        {
            cimg_library::CImg<unsigned short> img1 = decoder.decodeChannel(1);
            cimg_library::CImg<unsigned short> img2 = decoder.decodeChannel(2);
            cimg_library::CImg<unsigned short> img3 = decoder.decodeChannel(3);

            final_image.draw_image(0, 0, 0, 0, img2);
            final_image.draw_image(0, 0, 0, 1, img2);
            final_image.draw_image(0, 0, 0, 2, img1);
        }
        else
        {
            final_image = decoder.decodeChannel(valueChannel.getValue());
        }
    }
    else if (satelliteArg.getValue() == "METEOR")
    {
        // METEOR Decoding! MN2x
        std::cout << "Decoding METEOR!" << std::endl;

        METEORDecoder decoder(input_file);
        decoder.processHRPT();

        final_image = cimg_library::CImg<unsigned short>(1572, decoder.getTotalFrameCount(), 1, 3);

        if (optionFalseColor.getValue())
        {
            cimg_library::CImg<unsigned short> img1 = decoder.decodeChannel(1);
            cimg_library::CImg<unsigned short> img2 = decoder.decodeChannel(2);
            cimg_library::CImg<unsigned short> img3 = decoder.decodeChannel(3);

            final_image.draw_image(0, 0, 0, 0, img3);
            final_image.draw_image(0, 0, 0, 1, img2);
            final_image.draw_image(0, 0, 0, 2, img1);
        }
        else
        {
            final_image = decoder.decodeChannel(valueChannel.getValue());
        }

        decoder.cleanupFiles();
    }

    // Equalize and rotate if necessary
    final_image.equalize(valueEqualize.getValue());
    if (optionSouthbound.getValue())
        final_image.rotate(180);

    // Save our final image
    final_image.save_png(valueOutput.getValue().c_str());
    input_file.close();
}
