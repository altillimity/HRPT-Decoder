# HRPT Decoder

A C++ program to decode HRPT data from various satellites, currently only supporting NOAA (output from GNU Radio). It aims at being simple and light while still providing all the features needed.

### Usage
*Output off hrpt_decoder --help*

```
USAGE: 

   ./build/hrpt_decoder  {-c <channel>|-f} [-e <equalization>] [-S] -o
                         <image.png> -i <file> -t <NOAA|METEOR|MetOp
                         |FengYun> [--] [--version] [-h]


Where: 

   -c <channel>,  --channel <channel>
     (OR required)  Channel to extract
         -- OR --
   -f,  --falsecolor
     (OR required)  Produce false-color image


   -e <equalization>,  --equalization <equalization>
     Equalization to apply

   -S,  --southbound
     Southbound pass (defaults to Northbound)

   -o <image.png>,  --output <image.png>
     (required)  Output image file

   -i <file>,  --input <file>
     (required)  Raw input file

   -t <NOAA|METEOR|MetOp|FengYun>,  --type <NOAA|METEOR|MetOp|FengYun>
     (required)  Satellite to decode

   --,  --ignore_rest
     Ignores the rest of the labeled arguments following this flag.

   --version
     Displays version information and exits.

   -h,  --help
     Displays usage information and exits.


   HRPT Decoder by Aang23
```
