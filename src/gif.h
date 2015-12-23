//
// gif.h
// by Charlie Tangora
// Public domain.
// Email me : ctangora -at- gmail -dot- com
//
// This file offers a simple, very limited way to create animated GIFs directly in code.
//
// Those looking for particular cleverness are likely to be disappointed; it's pretty 
// much a straight-ahead implementation of the GIF format with optional Floyd-Steinberg 
// dithering. (It does at least use delta encoding - only the changed portions of each 
// frame are saved.) 
//
// So resulting files are often quite large. The hope is that it will be handy nonetheless
// as a quick and easily-integrated way for programs to spit out animations.
//
// Only RGBA8 is currently supported as an input format. (The alpha is ignored.)
//
// USAGE:
// Create a GifWriter struct. Pass it to GifBegin() to initialize and write the header.
// Pass subsequent frames to GifWriteFrame().
// Finally, call GifEnd() to close the file handle and free memory.
//

#ifndef gif_h
#define gif_h

#include <SoyPixels.h>
#include <string.h>  // for memcpy and bzero
#include <stdint.h>  // for integer typedefs

typedef vec3x<uint8> Rgb8;

// Define these macros to hook into a custom memory allocator.
// TEMP_MALLOC and TEMP_FREE will only be called in stack fashion - frees in the reverse order of mallocs
// and any temp memory allocated by a function will be freed before it exits.
// MALLOC and FREE are used only by GifBegin and GifEnd respectively (to allocate a buffer the size of the image, which
// is used to find changed pixels for delta-encoding.)

#ifndef GIF_TEMP_MALLOC
#include <stdlib.h>
#define GIF_TEMP_MALLOC malloc
#endif

#ifndef GIF_TEMP_FREE
#include <stdlib.h>
#define GIF_TEMP_FREE free
#endif

#ifndef GIF_MALLOC
#include <stdlib.h>
#define GIF_MALLOC malloc
#endif

#ifndef GIF_FREE
#include <stdlib.h>
#define GIF_FREE free
#endif

class GifWriter
{
public:
	GifWriter()
	{
	}
	
	std::function<void()>				Open;
	std::function<void(uint8)>			fputc;
	std::function<void(const char*)>	fputs;
	std::function<void(uint8*,size_t)>	fwrite;
 
	std::function<void()>				Close;
};

const int kGifTransIndex = 0;

class GifPalette
{
public:
	GifPalette() :
		bitDepth	( 8 )
	{
		mPalette.Init( GetSize(), 1, SoyPixelsFormat::RGB );
	}
	
    int bitDepth;	//	size= 1<<bitdepth

	size_t		GetSize() const
	{
		return 1<<bitDepth;
	}
					  
	Rgb8		GetColour(size_t Index) const
	{
		Rgb8 Rgb;
		Rgb.x = mPalette.GetPixel( Index, 0, 0 );
		Rgb.y = mPalette.GetPixel( Index, 0, 1 );
		Rgb.z = mPalette.GetPixel( Index, 0, 2 );
		return Rgb;
	}
	
	void		SetColour(size_t Index,Rgb8 rgb)
	{
		mPalette.SetPixel( Index, 0, 0, rgb.x );
		mPalette.SetPixel( Index, 0, 1, rgb.y );
		mPalette.SetPixel( Index, 0, 2, rgb.z );
	}

public:
	SoyPixels	mPalette;	//	Nx1 image of colours (RGB)
    
    // k-d tree over RGB space, organized in heap fashion
    // i.e. left child of node i is node i*2, right child is node i*2+1
    // nodes 256-511 are implicitly the leaves, containing a color
    uint8_t treeSplitElt[255];
    uint8_t treeSplit[255];
};

// max, min, and abs functions
int GifIMax(int l, int r) { return l>r?l:r; }
int GifIMin(int l, int r) { return l<r?l:r; }
int GifIAbs(int i) { return i<0?-i:i; }

// walks the k-d tree to pick the palette entry for a desired color.
// Takes as in/out parameters the current best color and its error -
// only changes them if it finds a better color in its subtree.
// this is the major hotspot in the code at the moment.
void GifGetClosestPaletteColor(GifPalette& Palette, int r, int g, int b, int& bestInd, int& bestDiff, int treeRoot = 1)
{
    // base case, reached the bottom of the tree
    if(treeRoot > (1<<Palette.bitDepth)-1)
    {
        int ind = treeRoot-(1<<Palette.bitDepth);
        if(ind == kGifTransIndex) return;
        
        // check whether this color is better than the current winner
		auto rgb = Palette.GetColour(ind);
		
        int r_err = r - (int)rgb.x;
        int g_err = g - (int)rgb.y;
        int b_err = b - (int)rgb.z;
        int diff = GifIAbs(r_err)+GifIAbs(g_err)+GifIAbs(b_err);
        
        if(diff < bestDiff)
        {
            bestInd = ind;
            bestDiff = diff;
        }
        
        return;
    }
    
    // take the appropriate color (r, g, or b) for this node of the k-d tree
    int comps[3]; comps[0] = r; comps[1] = g; comps[2] = b;
    int splitComp = comps[Palette.treeSplitElt[treeRoot]];
    
    int splitPos = Palette.treeSplit[treeRoot];
    if(splitPos > splitComp)
    {
        // check the left subtree
        GifGetClosestPaletteColor(Palette, r, g, b, bestInd, bestDiff, treeRoot*2);
        if( bestDiff > splitPos - splitComp )
        {
            // cannot prove there's not a better value in the right subtree, check that too
            GifGetClosestPaletteColor(Palette, r, g, b, bestInd, bestDiff, treeRoot*2+1);
        }
    }
    else
    {
        GifGetClosestPaletteColor(Palette, r, g, b, bestInd, bestDiff, treeRoot*2+1);
        if( bestDiff > splitComp - splitPos )
        {
            GifGetClosestPaletteColor(Palette, r, g, b, bestInd, bestDiff, treeRoot*2);
        }
    }
}

void GifSwapPixels(uint8_t* image, int pixA, int pixB)
{
    uint8_t rA = image[pixA*4];
    uint8_t gA = image[pixA*4+1];
    uint8_t bA = image[pixA*4+2];
    uint8_t aA = image[pixA*4+3];
    
    uint8_t rB = image[pixB*4];
    uint8_t gB = image[pixB*4+1];
    uint8_t bB = image[pixB*4+2];
    uint8_t aB = image[pixA*4+3];
    
    image[pixA*4] = rB;
    image[pixA*4+1] = gB;
    image[pixA*4+2] = bB;
    image[pixA*4+3] = aB;
    
    image[pixB*4] = rA;
    image[pixB*4+1] = gA;
    image[pixB*4+2] = bA;
    image[pixB*4+3] = aA;
}

// just the partition operation from quicksort
int GifPartition(uint8_t* image, const int left, const int right, const int elt, int pivotIndex)
{
    const int pivotValue = image[(pivotIndex)*4+elt];
    GifSwapPixels(image, pivotIndex, right-1);
    int storeIndex = left;
    bool split = 0;
    for(int ii=left; ii<right-1; ++ii)
    {
        int arrayVal = image[ii*4+elt];
        if( arrayVal < pivotValue )
        {
            GifSwapPixels(image, ii, storeIndex);
            ++storeIndex;
        }
        else if( arrayVal == pivotValue )
        {
            if(split)
            {
                GifSwapPixels(image, ii, storeIndex);
                ++storeIndex;
            }
            split = !split;
        }
    }
    GifSwapPixels(image, storeIndex, right-1);
    return storeIndex;
}

// Perform an incomplete sort, finding all elements above and below the desired median
void GifPartitionByMedian(uint8_t* image, int left, int right, int com, int neededCenter)
{
    if(left < right-1)
    {
        int pivotIndex = left + (right-left)/2;
    
        pivotIndex = GifPartition(image, left, right, com, pivotIndex);
        
        // Only "sort" the section of the array that contains the median
        if(pivotIndex > neededCenter)
            GifPartitionByMedian(image, left, pivotIndex, com, neededCenter);
        
        if(pivotIndex < neededCenter)
            GifPartitionByMedian(image, pivotIndex+1, right, com, neededCenter);
    }
}

// Builds a palette by creating a balanced k-d tree of all pixels in the image
void GifSplitPalette(uint8_t* image, int numPixels, int firstElt, int lastElt, int splitElt, int splitDist, int treeNode, bool buildForDither,GifPalette& Palette)
{
    if(lastElt <= firstElt || numPixels == 0)
        return;
    
    // base case, bottom of the tree
    if(lastElt == firstElt+1)
    {
        if(buildForDither)
        {
            // Dithering needs at least one color as dark as anything
            // in the image and at least one brightest color -
            // otherwise it builds up error and produces strange artifacts
            if( firstElt == 1 )
            {
                // special case: the darkest color in the image
                uint32_t r=255, g=255, b=255;
                for(int ii=0; ii<numPixels; ++ii)
                {
                    r = GifIMin(r, image[ii*4+0]);
                    g = GifIMin(g, image[ii*4+1]);
                    b = GifIMin(b, image[ii*4+2]);
                }
				
				Palette.SetColour( firstElt, Rgb8(r,g,b) );
				
                return;
            }
            
            if( firstElt == (1 << Palette.bitDepth)-1 )
            {
                // special case: the lightest color in the image
                uint32_t r=0, g=0, b=0;
                for(int ii=0; ii<numPixels; ++ii)
                {
                    r = GifIMax(r, image[ii*4+0]);
                    g = GifIMax(g, image[ii*4+1]);
                    b = GifIMax(b, image[ii*4+2]);
                }
				
				Palette.SetColour( firstElt, Rgb8(r,g,b) );
				
                return;
            }
        }
        
        // otherwise, take the average of all colors in this subcube
        uint64_t r=0, g=0, b=0;
        for(int ii=0; ii<numPixels; ++ii)
        {
            r += image[ii*4+0];
            g += image[ii*4+1];
            b += image[ii*4+2];
        }
        
        r += numPixels / 2;  // round to nearest
        g += numPixels / 2;
        b += numPixels / 2;
        
        r /= numPixels;
        g /= numPixels;
        b /= numPixels;
		
		Palette.SetColour( firstElt, Rgb8(r,g,b) );
		
        return;
    }
    
    // Find the axis with the largest range
    int minR = 255, maxR = 0;
    int minG = 255, maxG = 0;
    int minB = 255, maxB = 0;
    for(int ii=0; ii<numPixels; ++ii)
    {
        int r = image[ii*4+0];
        int g = image[ii*4+1];
        int b = image[ii*4+2];
        
        if(r > maxR) maxR = r;
        if(r < minR) minR = r;
        
        if(g > maxG) maxG = g;
        if(g < minG) minG = g;
        
        if(b > maxB) maxB = b;
        if(b < minB) minB = b;
    }
    
    int rRange = maxR - minR;
    int gRange = maxG - minG;
    int bRange = maxB - minB;
    
    // and split along that axis. (incidentally, this means this isn't a "proper" k-d tree but I don't know what else to call it)
    int splitCom = 1;
    if(bRange > gRange) splitCom = 2;
    if(rRange > bRange && rRange > gRange) splitCom = 0;
    
    int subPixelsA = numPixels * (splitElt - firstElt) / (lastElt - firstElt);
    int subPixelsB = numPixels-subPixelsA;
    
    GifPartitionByMedian(image, 0, numPixels, splitCom, subPixelsA);
    
    Palette.treeSplitElt[treeNode] = splitCom;
    Palette.treeSplit[treeNode] = image[subPixelsA*4+splitCom];
    
    GifSplitPalette(image,              subPixelsA, firstElt, splitElt, splitElt-splitDist, splitDist/2, treeNode*2,   buildForDither, Palette );
    GifSplitPalette(image+subPixelsA*4, subPixelsB, splitElt, lastElt,  splitElt+splitDist, splitDist/2, treeNode*2+1, buildForDither, Palette );
}

// Finds all pixels that have changed from the previous image and
// moves them to the fromt of th buffer.
// This allows us to build a palette optimized for the colors of the
// changed pixels only.
int GifPickChangedPixels(SoyPixelsImpl& PrevIndexes,GifPalette& PrevPalette, uint8_t* frame_rgba)
{
	Soy::Assert( PrevIndexes.GetFormat()==SoyPixelsFormat::Greyscale, "Expecting indexed iamge");
	
    int numChanged = 0;
    uint8_t* writeIter = frame_rgba;
	
	auto& LastIndexes = PrevIndexes.GetPixelsArray();
	int numPixels = LastIndexes.GetSize();
	
    for (int ii=0; ii<numPixels; ++ii)
    {
		auto Lastrgb = PrevPalette.GetColour( LastIndexes[ii] );
		auto r = frame_rgba[ii*4+0];
		auto g = frame_rgba[ii*4+1];
		auto b = frame_rgba[ii*4+2];
		Rgb8 ThisRgb( r,g,b);
		
		if( Lastrgb == ThisRgb )
		{
			
		}
		else
		{
            writeIter[0] = ThisRgb.x;
            writeIter[1] = ThisRgb.y;
            writeIter[2] = ThisRgb.z;
            ++numChanged;
            writeIter += 4;
        }
    }
    
    return numChanged;
}

// Creates a palette by placing all the image pixels in a k-d tree and then averaging the blocks at the bottom.
// This is known as the "modified median split" technique
void GifMakePalette(SoyPixelsImpl* PrevIndexes,GifPalette* PrevPalette, const uint8_t* nextFrame, uint32_t width, uint32_t height, bool buildForDither, GifPalette& Palette )
{
    // SplitPalette is destructive (it sorts the pixels by color) so
    // we must create a copy of the image for it to destroy
    int imageSize = width*height*4*sizeof(uint8_t);
    uint8_t* destroyableImage = (uint8_t*)GIF_TEMP_MALLOC(imageSize);
    memcpy(destroyableImage, nextFrame, imageSize);
    
    int numPixels = width*height;
    if(PrevIndexes && PrevPalette)
	{
        numPixels = GifPickChangedPixels(*PrevIndexes, *PrevPalette, destroyableImage );
	}
	
    const int lastElt = Palette.GetSize();
    const int splitElt = lastElt/2;
    const int splitDist = splitElt/2;
    
    GifSplitPalette(destroyableImage, numPixels, 1, lastElt, splitElt, splitDist, 1, buildForDither, Palette );
    
    GIF_TEMP_FREE(destroyableImage);
    
    // add the bottom node for the transparency index
    Palette.treeSplit[1 << (Palette.bitDepth-1)] = 0;
    Palette.treeSplitElt[1 << (Palette.bitDepth-1)] = 0;
	
	Palette.SetColour( 0, Rgb8(0,0,0) );
}

// Implements Floyd-Steinberg dithering, writes palette value to alpha
void GifDitherImage(SoyPixelsImpl* LastIndexes,GifPalette* LastPalette,const uint8_t* nextFrame,SoyPixelsImpl& OutImage, uint32_t width, uint32_t height, GifPalette& Palette )
{
	uint8_t* outFrame = OutImage.GetPixelsArray().GetArray();
	int numPixels = width*height;
    
    // quantPixels initially holds color*256 for all pixels
    // The extra 8 bits of precision allow for sub-single-color error values
    // to be propagated
    int32_t* quantPixels = (int32_t*)GIF_TEMP_MALLOC(sizeof(int32_t)*numPixels*4);
    
    for( int ii=0; ii<numPixels*4; ++ii )
    {
        uint8_t pix = nextFrame[ii];
        int32_t pix16 = int32_t(pix) * 256;
        quantPixels[ii] = pix16;
    }
    
    for( uint32_t yy=0; yy<height; ++yy )
    {
        for( uint32_t xx=0; xx<width; ++xx )
        {
            int32_t* nextPix = quantPixels + 4*(yy*width+xx);
			
			// Compute the colors we want (rounding to nearest)
			int32_t rr = (nextPix[0] + 127) / 256;
			int32_t gg = (nextPix[1] + 127) / 256;
			int32_t bb = (nextPix[2] + 127) / 256;

			bool SamePixel = false;
			if ( LastIndexes && LastPalette )
			{
				auto LastRgb = LastPalette->GetColour( LastIndexes->GetPixel(xx,yy,0) );
				Rgb8 NewRgb( rr, gg, bb );
				SamePixel = LastRgb == NewRgb;
			}
			
			
            // if it happens that we want the color from last frame, then just write out
            // a transparent pixel
            if( SamePixel )
            {
                nextPix[0] = rr;
                nextPix[1] = gg;
                nextPix[2] = bb;
                nextPix[3] = kGifTransIndex;
                continue;
            }
            
            int32_t bestDiff = 1000000;
            int32_t bestInd = kGifTransIndex;
            
            // Search the palete
            GifGetClosestPaletteColor( Palette, rr, gg, bb, bestInd, bestDiff);
            
            // Write the result to the temp buffer
			auto Bestrgb = Palette.GetColour( bestInd );
            int32_t r_err = nextPix[0] - int32_t(Bestrgb.x) * 256;
            int32_t g_err = nextPix[1] - int32_t(Bestrgb.y) * 256;
            int32_t b_err = nextPix[2] - int32_t(Bestrgb.z) * 256;
            
            nextPix[0] = Bestrgb.x;
            nextPix[1] = Bestrgb.y;
            nextPix[2] = Bestrgb.z;
            nextPix[3] = bestInd;
            
            // Propagate the error to the four adjacent locations
            // that we haven't touched yet
            int quantloc_7 = (yy*width+xx+1);
            int quantloc_3 = (yy*width+width+xx-1);
            int quantloc_5 = (yy*width+width+xx);
            int quantloc_1 = (yy*width+width+xx+1);
            
            if(quantloc_7 < numPixels)
            {
                int32_t* pix7 = quantPixels+4*quantloc_7;
                pix7[0] += GifIMax( -pix7[0], r_err * 7 / 16 );
                pix7[1] += GifIMax( -pix7[1], g_err * 7 / 16 );
                pix7[2] += GifIMax( -pix7[2], b_err * 7 / 16 );
            }
            
            if(quantloc_3 < numPixels)
            {
                int32_t* pix3 = quantPixels+4*quantloc_3;
                pix3[0] += GifIMax( -pix3[0], r_err * 3 / 16 );
                pix3[1] += GifIMax( -pix3[1], g_err * 3 / 16 );
                pix3[2] += GifIMax( -pix3[2], b_err * 3 / 16 );
            }
            
            if(quantloc_5 < numPixels)
            {
                int32_t* pix5 = quantPixels+4*quantloc_5;
                pix5[0] += GifIMax( -pix5[0], r_err * 5 / 16 );
                pix5[1] += GifIMax( -pix5[1], g_err * 5 / 16 );
                pix5[2] += GifIMax( -pix5[2], b_err * 5 / 16 );
            }
            
            if(quantloc_1 < numPixels)
            {
                int32_t* pix1 = quantPixels+4*quantloc_1;
                pix1[0] += GifIMax( -pix1[0], r_err / 16 );
                pix1[1] += GifIMax( -pix1[1], g_err / 16 );
                pix1[2] += GifIMax( -pix1[2], b_err / 16 );
            }
        }
    }
    
    // Copy the palettized result to the output buffer
    for( int ii=0; ii<numPixels*4; ++ii )
    {
        outFrame[ii] = quantPixels[ii];
    }
    
    GIF_TEMP_FREE(quantPixels);
}

// Picks palette colors for the image using simple thresholding, no dithering
void GifThresholdImage(SoyPixelsImpl* LastIndexes,GifPalette* LastPalette,const uint8_t* nextFrame,SoyPixelsImpl& OutImage, uint32_t width, uint32_t height, GifPalette& Palette )
{
	uint8_t* outFrame = OutImage.GetPixelsArray().GetArray();
	
	Array<uint8> Dummy;
	
	for( uint32_t yy=0; yy<height; ++yy )
		for( uint32_t xx=0; xx<width; ++xx )
    {
		int Index = (yy*width*4) + (xx*4);
        // if a previous color is available, and it matches the current color,
        // set the pixel to transparent
		bool SamePixel = false;
		Rgb8 NextRgb( nextFrame[Index+0], nextFrame[Index+1], nextFrame[Index+2] );
		Rgb8 LastRgb;
		
		if ( LastIndexes && LastPalette )
		{
			LastRgb = LastPalette->GetColour( LastIndexes->GetPixel(xx,yy,0) );
			SamePixel = LastRgb == NextRgb;
		}

		if ( SamePixel )
		{
            outFrame[Index+0] = LastRgb.x;
            outFrame[Index+1] = LastRgb.y;
            outFrame[Index+2] = LastRgb.z;
            outFrame[Index+3] = kGifTransIndex;
        }
        else
        {
            // palettize the pixel
            int32_t bestDiff = 1000000;
            int32_t bestInd = 1;
            GifGetClosestPaletteColor( Palette, NextRgb.x, NextRgb.y, NextRgb.z, bestInd, bestDiff);
            
            // Write the resulting color to the output buffer
			auto rgb = Palette.GetColour( bestInd );
			outFrame[Index+0] = rgb.x;
			outFrame[Index+1] = rgb.y;
			outFrame[Index+2] = rgb.z;
            outFrame[Index+3] = bestInd;
        }
	}
}

// Simple structure to write out the LZW-compressed portion of the image
// one bit at a time
struct GifBitStatus
{
    uint8_t bitIndex;  // how many bits in the partial byte written so far
    uint8_t byte;      // current partial byte
    
    uint32_t chunkIndex;
    uint8_t chunk[256];   // bytes are written in here until we have 256 of them, then written to the file
};

// insert a single bit
void GifWriteBit( GifBitStatus& stat, uint32_t bit )
{
    bit = bit & 1;
    bit = bit << stat.bitIndex;
    stat.byte |= bit;
    
    ++stat.bitIndex;
    if( stat.bitIndex > 7 )
    {
        // move the newly-finished byte to the chunk buffer 
        stat.chunk[stat.chunkIndex++] = stat.byte;
        // and start a new byte
        stat.bitIndex = 0;
        stat.byte = 0;
    }
}

// write all bytes so far to the file
void GifWriteChunk(GifWriter& Writer, GifBitStatus& stat )
{
    Writer.fputc(stat.chunkIndex);
    Writer.fwrite(stat.chunk, stat.chunkIndex);
    
    stat.bitIndex = 0;
    stat.byte = 0;
    stat.chunkIndex = 0;
}

void GifWriteCode(GifWriter& Writer, GifBitStatus& stat, uint32_t code, uint32_t length )
{
    for( uint32_t ii=0; ii<length; ++ii )
    {
        GifWriteBit(stat, code);
        code = code >> 1;
        
        if( stat.chunkIndex == 255 )
        {
            GifWriteChunk( Writer, stat);
        }
    }
}

// The LZW dictionary is a 256-ary tree constructed as the file is encoded,
// this is one node
struct GifLzwNode
{
    uint16_t m_next[256];
};

// write a 256-color (8-bit) image palette to the file
void GifWritePalette( const GifPalette& Palette,GifWriter& Writer)
{
    Writer.fputc(0);  // first color: transparency
    Writer.fputc(0);
    Writer.fputc(0);
    
    for(int ii=1; ii<Palette.GetSize(); ++ii)
    {
		auto rgb = Palette.GetColour(ii);
        Writer.fputc(rgb.x);
        Writer.fputc(rgb.y);
        Writer.fputc(rgb.z);
    }
}

// write the image header, LZW-compress and write out the image
void GifWriteLzwImage(GifWriter& Writer,const SoyPixelsImpl& Image, uint32_t left, uint32_t top,uint32_t delay, GifPalette& Palette)
{
	Soy::Assert( Image.GetFormat()==SoyPixelsFormat::Greyscale, "Expecting palette-index iamge format");
	auto width = Image.GetWidth();
	auto height = Image.GetHeight();
	
    // graphics control extension
    Writer.fputc(0x21);
    Writer.fputc(0xf9);
    Writer.fputc(0x04);
    Writer.fputc(0x05); // leave prev frame in place, this frame has transparency
    Writer.fputc(delay & 0xff);
    Writer.fputc((delay >> 8) & 0xff);
    Writer.fputc(kGifTransIndex); // transparent color index
    Writer.fputc(0);
    
    Writer.fputc(0x2c); // image descriptor block
    
    Writer.fputc(left & 0xff);           // corner of image in canvas space
    Writer.fputc((left >> 8) & 0xff);
    Writer.fputc(top & 0xff);
    Writer.fputc((top >> 8) & 0xff);
    
    Writer.fputc(width & 0xff);          // width and height of image
	Writer.fputc((width >> 8) & 0xff);
    Writer.fputc(height & 0xff);
    Writer.fputc((height >> 8) & 0xff);
    
    //fputc(0); // no local color table, no transparency
    //fputc(0x80); // no local color table, but transparency
    
    Writer.fputc(0x80 + Palette.bitDepth-1); // local color table present, 2 ^ bitDepth entries
    GifWritePalette(Palette,Writer);
    
    const int minCodeSize = Palette.bitDepth;
	const uint32_t clearCode = size_cast<const uint32_t>( Palette.GetSize() );
	
    Writer.fputc(minCodeSize); // min code size 8 bits
    
    GifLzwNode* codetree = (GifLzwNode*)GIF_TEMP_MALLOC(sizeof(GifLzwNode)*4096);
    
    memset(codetree, 0, sizeof(GifLzwNode)*4096);
    int32_t curCode = -1;
    uint32_t codeSize = minCodeSize+1;
    uint32_t maxCode = clearCode+1;
    
    GifBitStatus stat;
    stat.byte = 0;
    stat.bitIndex = 0;
    stat.chunkIndex = 0;
    
    GifWriteCode(Writer,stat, clearCode, codeSize);  // start with a fresh LZW dictionary
	
	auto& Indexes = Image.GetPixelsArray();
	
    for(uint32_t yy=0; yy<height; ++yy)
    {
        for(uint32_t xx=0; xx<width; ++xx)
        {
            uint8_t nextValue = Indexes[yy*width+xx];
            
            // "loser mode" - no compression, every single code is followed immediately by a clear
            //WriteCode( f, stat, nextValue, codeSize );
            //WriteCode( f, stat, 256, codeSize );
            
            if( curCode < 0 )
            {
                // first value in a new run
                curCode = nextValue;
            }
            else if( codetree[curCode].m_next[nextValue] )
            {
                // current run already in the dictionary
                curCode = codetree[curCode].m_next[nextValue];
            }
            else
            {
                // finish the current run, write a code
                GifWriteCode( Writer, stat, curCode, codeSize );
                
                // insert the new run into the dictionary
                codetree[curCode].m_next[nextValue] = ++maxCode;
                
                if( maxCode >= (1ul << codeSize) )
                {
                    // dictionary entry count has broken a size barrier,
                    // we need more bits for codes
                    codeSize++;
                }
                if( maxCode == 4095 )
                {
                    // the dictionary is full, clear it out and begin anew
                    GifWriteCode(Writer, stat, clearCode, codeSize); // clear tree
                    
                    memset(codetree, 0, sizeof(GifLzwNode)*4096);
                    curCode = -1;
                    codeSize = minCodeSize+1;
                    maxCode = clearCode+1;
                }
                
                curCode = nextValue;
            }
        }
    }
    
    // compression footer
    GifWriteCode( Writer, stat, curCode, codeSize );
    GifWriteCode( Writer, stat, clearCode, codeSize );
    GifWriteCode( Writer, stat, clearCode+1, minCodeSize+1 );
    
    // write out the last partial chunk
    while( stat.bitIndex ) GifWriteBit(stat, 0);
    if( stat.chunkIndex ) GifWriteChunk(Writer, stat);
    
	Writer.fputc(0); // image block terminator
    
    GIF_TEMP_FREE(codetree);
}



// Creates a gif file.
// The input GIFWriter is assumed to be uninitialized.
// The delay value is the time between frames in hundredths of a second - note that not all viewers pay much attention to this value.
bool GifBegin( GifWriter& writer, uint32_t width, uint32_t height, uint32_t delay, int32_t bitDepth = 8, bool dither = false )
{
	writer.Open();
	
    writer.fputs("GIF89a");
    
    // screen descriptor
    writer.fputc(width & 0xff);
    writer.fputc((width >> 8) & 0xff);
    writer.fputc(height & 0xff);
    writer.fputc((height >> 8) & 0xff);
    
    writer.fputc(0xf0);  // there is an unsorted global color table of 2 entries
    writer.fputc(0);     // background color
    writer.fputc(0);     // pixels are square (we need to specify this because it's 1989)
    
    // now the "global" palette (really just a dummy palette)
    // color 0: black
    writer.fputc(0);
    writer.fputc(0);
    writer.fputc(0);
    // color 1: also black
    writer.fputc(0);
    writer.fputc(0);
    writer.fputc(0);
    
    if( delay != 0 )
    {
		// animation header
		writer.fputc(0x21); // extension
		writer.fputc(0xff); // application specific
		writer.fputc(11); // length 11
		writer.fputs("NETSCAPE2.0"); // yes, really
		writer.fputc(3); // 3 bytes of NETSCAPE2.0 data

		writer.fputc(1); // JUST BECAUSE
		writer.fputc(0); // loop infinitely (byte 0)
		writer.fputc(0); // loop infinitely (byte 1)

		writer.fputc(0); // block terminator
	}
	
    return true;
}


// Writes the EOF code, closes the file handle, and frees temp memory used by a GIF.
// Many if not most viewers will still display a GIF properly if the EOF code is missing,
// but it's still a good idea to write it out.
void GifEnd( GifWriter& writer )
{
	writer.fputc(0x3b); // end of file
	writer.Close();
}

#endif
