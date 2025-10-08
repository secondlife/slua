//
// The Great Computer Language Shootout
// http://shootout.alioth.debian.org/
//
// contributed by Isaac Gouy
// modified by Babbage Linden, Oct 10 2007
//

string hexc="0123456789ABCDEF";

string byte2hex(integer byte) 
{
    integer highBits = (byte & 0xF0) >> 4;
    integer lowBits = byte & 0x0F;
    return llGetSubString(hexc, highBits, highBits) + llGetSubString(hexc, lowBits, 
lowBits);
}

mandlebrot(integer width)
{
    integer height = width;
    integer i;
    integer m = 50;
    integer bits = 0;
    integer bitnum = 0;
    integer isOverLimit = FALSE;
    float Zr = 0.0;
    float Zi = 0.0;
    float Cr = 0.0;
    float Ci = 0.0;
    float Tr;
    float Ti;
    float limit2 = 4.0;
    
    print("P4");
    print((string)width + " " + (string)height);
    
    string hexBytes = "";
    
    integer y;
    for(y = 0; y < height; y++) 
    {
        integer x;
     for(x = 0; x < width; x++)
     {
    
        Zr = 0.0; Zi = 0.0;
        Cr = 2.0*x / width - 1.5;
        Ci = 2.0*y / height - 1.0;
    
        i = 0;
        do {
           Tr = Zr*Zr - Zi*Zi + Cr;
           Ti = 2.0*Zr*Zi + Ci;
           Zr = Tr; Zi = Ti;
           isOverLimit = Zr*Zr + Zi*Zi > limit2;
        } while (!isOverLimit && (++i < m));
    
        bits = bits << 1;
        if (!isOverLimit) bits++;
        bitnum++;
    
        if (x == width - 1) {
           bits = bits << (8 - bitnum);
           bitnum = 8;
        }
    
        if (bitnum == 8)
        {
           hexBytes += byte2hex(bits);
           bits = 0; bitnum = 0;
        }
     }
    }
    print("0x" + hexBytes);
}

test()
{
    mandlebrot(10);
}

default
{
    state_entry()
    {
        test();
    }
}
