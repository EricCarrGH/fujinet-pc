#include "html_printer.h"
#include "../../include/atascii.h"
#include "../../include/debug.h"


int AtasciiToStandardUnicode[128] = 
{
    9829, 9500, 9621, 9496, 9508, 9488, 9585, 9586,   //   8
    9698, 9623, 9699, 9629, 9624, 9620, 9601, 9622,   //  16
    9827, 9484, 9472, 9532, 11044, 9604, 9615, 9516,  //  24
    9524, 9612, 9492, 8455, 8593, 8595, 8592, 8594,   //  32
    0, 0, 0, 0, 0, 0, 38, 0,                          //  40
    0, 0, 0, 0, 0, 0, 0, 0,                           //  48
    0, 0, 0, 0, 0, 0, 0, 0,                           //  56
    0, 0, 0, 0, 60, 0, 62, 0,                         //  64
    0, 0, 0, 0, 0, 0, 0, 0,                           //  72
    0, 0, 0, 0, 0, 0, 0, 0,                           //  80
    0, 0, 0, 0, 0, 0, 0, 0,                           //  88
    0, 0, 0, 0, 0, 0, 0, 0,                           //  96
    9830, 0, 0, 0, 0, 0, 0, 0,                        // 104
    0, 0, 0, 0, 0, 0, 0, 0,                           // 112
    0, 0, 0, 0, 0, 0, 0, 0,                           // 120
    0, 0, 0, 9824, 9475, 8598, 9658, 9668             // 128
};

void htmlPrinter::initPrinter(FS *filesystem)
{
    printer_emu::initPrinter(filesystem);
}

htmlPrinter::~htmlPrinter()
{
#ifdef DEBUG
    Debug_println("~htmlPrinter");
#endif
}

bool htmlPrinter::process(byte n)
{
    int i = 0;
    
    const char *inv_on = "<span class=\"iv\">";
    const char *inv_off = "</span>";
    const char *escape = "&#x%x;";

    while (i < n)
    {
        uint8_t c = buffer[i];

        // ATASCII_EOL is a line break and also a record break
        // (so ignore the rest of the buffer)
        if (c == ATASCII_EOL)
        {
            if(inverse) {
                inverse = false;
                _file.write((const uint8_t *)inv_off, strlen(inv_off));
            }
            _file.print("\r\n");
            break;
        }

        // Handle differently depending on wether we're doing this with an Atari font
        if(paperType == HTML_ATASCII)
        {
            // If it's a printable character, just copy it
            if(c >=32 && c <= 122 && c != 96)
            {
                // One more check to make sure it's not one of the HTML characters we have to escape
                if(AtasciiToStandardUnicode[c] != 0)
                    _file.printf(escape, AtasciiToStandardUnicode[c]);
                else
                    _file.write(c);
            }
            // It's an ATASCII character, so map it into the Private Use Area of the Atari font
            // PUA starts at 0xE000
            else
            {
                _file.printf(escape, 0xE000 + c);
            }

        }
        else
        {
            // If this is an inverse character, turn on inverse mode if it
            // isn't already on and convert the character to non-inverse
            if(c >= 128)
            {
                if(false == inverse)
                {
                    inverse = true;
                    _file.write((const uint8_t *)inv_on, strlen(inv_on));                
                }
                c -= 128;
            }
            // If it wasn't an inverse character and inverse was on, turn it off
            else if(inverse)
            {
                inverse = false;
                _file.write((const uint8_t *)inv_off, strlen(inv_off));
            }

            // If we have a replacement HTML entitiy for this character, use that
            if(AtasciiToStandardUnicode[c] != 0)
            {
                _file.printf(escape, AtasciiToStandardUnicode[c]);
            }
            // Otherwise just write the character
            else 
            {
                _file.write(c);
            }
        }

        i++;
    }
    return true;
}

void htmlPrinter::pre_page_eject()
{
    const char *htm = "</html>";
    _file.write((const uint8_t *)htm, strlen(htm));
    inverse = false;
}

void htmlPrinter::post_new_file()
{
    const char *htm1 = "<html><head><meta charset=\"utf-8\"/>\r\n<style>";
    const char *css = "body{font-family:\"Courier New\",Courier,monospace;white-space:pre;} .iv{color:white;background-color:black;}\r\n";
    const char *htm2 = "</style></head>\r\n";

    _file.write((const uint8_t *)htm1, strlen(htm1));

    // Load CSS that embeds the Atari font (or don't)
    if(paperType == HTML_ATASCII)
        copy_file_to_output("/atarifont.css");
    else
        _file.write((const uint8_t *)css, strlen(css));

    _file.write((const uint8_t *)htm2, strlen(htm2));        
}

void htmlPrinter::pageEject()
{
    printer_emu::pageEject();
}