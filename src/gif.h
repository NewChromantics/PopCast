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

typedef Soy::TRgb8 Rgb8;

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

inline bool isPowerOfTwo(size_t x)
{
	return ((x != 0) && !(x & (x - 1)));
}

uint32 GetNextPowerOfTwo(uint32 n)
{
	//	http://stackoverflow.com/a/1322548/355753
	n--;
	n |= n >> 1;   // Divide by 2^k for consecutive doublings of k up to 32,
	n |= n >> 2;   // and then or the results.
	n |= n >> 4;
	n |= n >> 8;
	n |= n >> 16;
	// The result is a number of 1 bits equal to the number
	// of bits in the original number, plus 1. That's the
	// next highest power of 2.
	n++;
	return n;
}

inline size_t GetBitIndex(size_t Integer)
{
	//	gr: completely forgotten what the function is called to work out MSB
	switch ( Integer )
	{
		case 1<<0:	return 0;
		case 1<<1:	return 1;
		case 1<<2:	return 2;
		case 1<<3:	return 3;
		case 1<<4:	return 4;
		case 1<<5:	return 5;
		case 1<<6:	return 6;
		case 1<<7:	return 7;
		case 1<<8:	return 8;
		default:
			break;
	}
	std::stringstream Error;
	Error << __func__ << " Integer is not power, cannot work out bit: " << Integer;
	throw Soy::AssertException( Error.str() );
}

class GifPalette
{
public:
	GifPalette(size_t ColourCount)
	{
		if ( !isPowerOfTwo(ColourCount) && ColourCount <= 256 )
		{
			std::stringstream Error;
			Error << "Palette size must be power of 2 (and 256 max); " << ColourCount;
			throw Soy::AssertException( Error.str() );
		}
		mPalette.reset( new SoyPixels );
		auto& Pal = GetPalette();
		Pal.Init( ColourCount, 1, SoyPixelsFormat::RGB );
	}
	
	GifPalette(std::shared_ptr<SoyPixelsImpl>& Palette) :
		mPalette	( Palette )
	{
		Soy::Assert( mPalette!=nullptr, "null palette");
		auto ColourCount = GetSize();
		if ( !isPowerOfTwo(ColourCount) && ColourCount <= 256 )
		{
			std::stringstream Error;
			Error << "Palette size must be power of 2 (and 256 max); " << ColourCount;
			throw Soy::AssertException( Error.str() );
		}
	}

	uint8			GetTransparentIndex() const
	{
		return 0;
	}
	
	SoyPixelsImpl&	GetPalette()
	{
		return *mPalette;
	}
	
	const SoyPixelsImpl&	GetPalette() const
	{
		return *mPalette;
	}
	
	size_t		GetSize() const
	{
		auto& Pal = GetPalette();
		return Pal.GetWidth() * Pal.GetHeight();
	}
					  
	Rgb8		GetColour(size_t Index) const
	{
		auto& Pal = GetPalette();
		return Pal.GetPixel3( Index, 0 );
	}
	
	void		SetColour(size_t Index,Rgb8 rgb)
	{
		auto& Pal = GetPalette();
		Pal.SetPixel( Index, 0, rgb );
	}

public:
	std::shared_ptr<SoyPixelsImpl>	mPalette;	//	Nx1 image of colours (RGB)
    
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
    if(treeRoot > Palette.GetSize()-1)
    {
        int ind = treeRoot - size_cast<int>(Palette.GetSize());
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

void GifSwapPixels(uint8_t* image,size_t ChannelCount, int pixA, int pixB)
{
	uint8 Temp[10];
	auto* PtrA = &image[pixA*ChannelCount];
	auto* PtrB = &image[pixB*ChannelCount];

	memcpy( Temp, PtrA, ChannelCount );
	memcpy( PtrA, PtrB, ChannelCount );
	memcpy( PtrB, Temp, ChannelCount );
}

// just the partition operation from quicksort
int GifPartition(uint8_t* image,size_t ChannelCount,const int left, const int right, const int elt, int pivotIndex)
{
    const int pivotValue = image[(pivotIndex)*ChannelCount+elt];
    GifSwapPixels(image, ChannelCount, pivotIndex, right-1);
    int storeIndex = left;
    bool split = 0;
    for(int ii=left; ii<right-1; ++ii)
    {
        int arrayVal = image[ii*ChannelCount+elt];
        if( arrayVal < pivotValue )
        {
            GifSwapPixels(image, ChannelCount, ii, storeIndex);
            ++storeIndex;
        }
        else if( arrayVal == pivotValue )
        {
            if(split)
            {
                GifSwapPixels(image, ChannelCount, ii, storeIndex);
                ++storeIndex;
            }
            split = !split;
        }
    }
    GifSwapPixels(image, ChannelCount, storeIndex, right-1);
    return storeIndex;
}

// Perform an incomplete sort, finding all elements above and below the desired median
void GifPartitionByMedian(uint8_t* image,size_t ChannelCount, int left, int right, int com, int neededCenter)
{
    if(left < right-1)
    {
        int pivotIndex = left + (right-left)/2;
    
        pivotIndex = GifPartition(image, ChannelCount, left, right, com, pivotIndex);
        
        // Only "sort" the section of the array that contains the median
        if(pivotIndex > neededCenter)
            GifPartitionByMedian(image, ChannelCount, left, pivotIndex, com, neededCenter);
        
        if(pivotIndex < neededCenter)
            GifPartitionByMedian(image, ChannelCount, pivotIndex+1, right, com, neededCenter);
    }
}

// Builds a palette by creating a balanced k-d tree of all pixels in the image
void GifSplitPalette(uint8_t* image,size_t ChannelCount, int numPixels, int firstElt, int lastElt, int splitElt, int splitDist, int treeNode, bool buildForDither,GifPalette& Palette)
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
                    r = GifIMin(r, image[ii*ChannelCount+0]);
                    g = GifIMin(g, image[ii*ChannelCount+1]);
                    b = GifIMin(b, image[ii*ChannelCount+2]);
                }
				
				Palette.SetColour( firstElt, Rgb8(r,g,b) );
				
                return;
            }
            
            if( firstElt == Palette.GetSize()-1 )
            {
                // special case: the lightest color in the image
                uint32_t r=0, g=0, b=0;
                for(int ii=0; ii<numPixels; ++ii)
                {
                    r = GifIMax(r, image[ii*ChannelCount+0]);
                    g = GifIMax(g, image[ii*ChannelCount+1]);
                    b = GifIMax(b, image[ii*ChannelCount+2]);
                }
				
				Palette.SetColour( firstElt, Rgb8(r,g,b) );
				
                return;
            }
        }
        
        // otherwise, take the average of all colors in this subcube
        uint64_t r=0, g=0, b=0;
        for(int ii=0; ii<numPixels; ++ii)
        {
            r += image[ii*ChannelCount+0];
            g += image[ii*ChannelCount+1];
            b += image[ii*ChannelCount+2];
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
        int r = image[ii*ChannelCount+0];
        int g = image[ii*ChannelCount+1];
        int b = image[ii*ChannelCount+2];
        
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
    
    GifPartitionByMedian(image, ChannelCount, 0, numPixels, splitCom, subPixelsA);
    
    Palette.treeSplitElt[treeNode] = splitCom;
    Palette.treeSplit[treeNode] = image[subPixelsA*ChannelCount+splitCom];
    
    GifSplitPalette(image, ChannelCount,          subPixelsA, firstElt, splitElt, splitElt-splitDist, splitDist/2, treeNode*2,   buildForDither, Palette );
    GifSplitPalette(image+subPixelsA*ChannelCount, ChannelCount, subPixelsB, splitElt, lastElt,  splitElt+splitDist, splitDist/2, treeNode*2+1, buildForDither, Palette );
}

void GifMakeDiffPalette(const SoyPixelsImpl& PrevIndexes,const SoyPixelsImpl& PrevPalette,const uint8_t* frame_rgba, uint32_t width, uint32_t height,SoyPixelsImpl& Palette)
{
	Soy::Assert( PrevIndexes.GetFormat()==SoyPixelsFormat::Greyscale, "Expecting indexed iamge");
	Soy::Assert( PrevPalette.GetFormat()==SoyPixelsFormat::RGB, "Expecting RGB palette");

	auto& LastIndexes = PrevIndexes.GetPixelsArray();
	auto numPixels = LastIndexes.GetSize();
	Array<Rgb8> NewColours;

	for (int ii=0; ii<numPixels; ++ii)
	{
		auto LastIndex = LastIndexes[ii];
		auto Lastrgb = PrevPalette.GetPixel3( LastIndex, 0 );
		auto r = frame_rgba[ii*4+0];
		auto g = frame_rgba[ii*4+1];
		auto b = frame_rgba[ii*4+2];
		Rgb8 ThisRgb( r,g,b);
			
		if( Lastrgb == ThisRgb )
		{
			//	skip this colour
		}
		else
		{
			NewColours.PushBack( ThisRgb );
		}
	}
	
	//	make a palette image
	Palette.Init( NewColours.GetSize(), 1, SoyPixelsFormat::RGB );
	auto NewColoursPixels = GetRemoteArray( reinterpret_cast<uint8*>( NewColours.GetArray() ), NewColours.GetDataSize() );
	Palette.GetPixelsArray().Copy( NewColoursPixels );
}



void GifDebugPalette(SoyPixelsImpl& Palette)
{
	Array<Rgb8> NewColours;
	static int DebugPaletteSize = 256;
	
	for ( int i=0;	i<DebugPaletteSize;	i++ )
	{
		Rgb8 ThisRgb( 255, i, 0 );
		NewColours.PushBack( ThisRgb );
	}
	/*
	for ( int i=0;	i<256;	i++ )
	{
		Rgb8 ThisRgb( 0, 255, i );
		NewColours.PushBack( ThisRgb );
	}
	 */
	
	//	make a palette image
	Palette.Init( NewColours.GetSize(), 1, SoyPixelsFormat::RGB );
	auto NewColoursPixels = GetRemoteArray( reinterpret_cast<uint8*>( NewColours.GetArray() ), NewColours.GetDataSize() );
	Palette.GetPixelsArray().Copy( NewColoursPixels );
}


// Creates a palette by placing all the image pixels in a k-d tree and then averaging the blocks at the bottom.
// This is known as the "modified median split" technique
void GifMakePalette(const SoyPixelsImpl& BigPalette,bool buildForDither,std::shared_ptr<GifPalette>& pPalette,size_t PaletteSize)
{
	Soy::Assert( BigPalette.GetFormat()==SoyPixelsFormat::RGB, "Expecting palette in RGB");

	pPalette.reset( new GifPalette(PaletteSize) );
	auto& SmallPalette = *pPalette;
	
	// SplitPalette is destructive (it sorts the pixels by color) so
	// we must create a copy of the image for it to destroy
	Array<uint8> destroyableImage;
	destroyableImage.Copy( BigPalette.GetPixelsArray() );
	auto* pData = destroyableImage.GetArray();
	
	int firstElement = 0;
	const int lastElt = size_cast<int>(SmallPalette.GetSize());
	const int splitElt = lastElt/2;
	const int splitDist = splitElt/2;
	int numPixels = size_cast<int>(BigPalette.GetWidth());
	int treeNode = 1;
	
	GifSplitPalette( pData, BigPalette.GetChannels(), numPixels, firstElement, lastElt, splitElt, splitDist, treeNode, buildForDither, SmallPalette );
	
	
	// add the bottom node for the transparency index
	//	gr: this doesn't look right to me
	SmallPalette.treeSplit[SmallPalette.GetSize()>>1] = 0;
	SmallPalette.treeSplitElt[SmallPalette.GetSize()>>1] = 0;
}

// Implements Floyd-Steinberg dithering, writes palette value to alpha
void GifDitherImage(SoyPixelsImpl* LastIndexes,SoyPixelsImpl* LastPalette,const uint8_t* nextFrame,SoyPixelsImpl& OutImage, uint32_t width, uint32_t height, GifPalette& Palette )
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
				auto LastRgb = LastPalette->GetPixel3( LastIndexes->GetPixel(xx,yy,0), 0 );
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
void GifThresholdImage(SoyPixelsImpl* LastIndexes,SoyPixelsImpl* LastPalette,const uint8_t* nextFrame,SoyPixelsImpl& OutIndexes, uint32_t width, uint32_t height, GifPalette& Palette )
{
	Soy::Assert( OutIndexes.GetFormat() == SoyPixelsFormat::Greyscale, "Output should be indexes" );
	uint8_t* outIndexes = OutIndexes.GetPixelsArray().GetArray();
	
	Array<uint8> Dummy;
	
	for( uint32_t yy=0; yy<height; ++yy )
		for( uint32_t xx=0; xx<width; ++xx )
    {
		int RgbaIndex = (yy*width*4) + (xx*4);
		int PalIndex = (yy*width) + (xx);
		
        // if a previous color is available, and it matches the current color,
        // set the pixel to transparent
		bool SamePixel = false;
		Rgb8 NextRgb( nextFrame[RgbaIndex+0], nextFrame[RgbaIndex+1], nextFrame[RgbaIndex+2] );
		Rgb8 LastRgb;
		
		if ( LastIndexes && LastPalette )
		{
			LastRgb = LastPalette->GetPixel3( LastIndexes->GetPixel(xx,yy,0), 0 );
			SamePixel = LastRgb == NextRgb;
		}

		if ( SamePixel )
		{
            outIndexes[PalIndex] = kGifTransIndex;
        }
        else
        {
            // palettize the pixel
            int32_t bestDiff = 1000000;
            int32_t bestInd = 1;
            GifGetClosestPaletteColor( Palette, NextRgb.x, NextRgb.y, NextRgb.z, bestInd, bestDiff);
            
            // Write the resulting color to the output buffer
			outIndexes[PalIndex] = bestInd;
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

void GifWritePalette( const SoyPixelsImpl& Palette,size_t PaddedPaletteSize,GifWriter& Writer)
{
	for( size_t ii=0; ii<Palette.GetWidth(); ++ii)
	{
		auto rgb = Palette.GetPixel3( ii, 0 );
		Writer.fputc(rgb.x);
		Writer.fputc(rgb.y);
		Writer.fputc(rgb.z);
	}
	
	//	padding colour
	for( size_t ii=Palette.GetWidth(); ii<PaddedPaletteSize; ++ii)
	{
		Rgb8 rgb( 1, 255, 255 );
		Writer.fputc(rgb.x);
		Writer.fputc(rgb.y);
		Writer.fputc(rgb.z);
	}
}

// write the image header, LZW-compress and write out the image
void GifWriteLzwImage(GifWriter& Writer,const SoyPixelsImpl& Image, uint16 left, uint16 top,uint16 delay,const SoyPixelsImpl& Palette,uint8 TransparentIndex)
{
	Soy::Assert( Image.GetFormat()==SoyPixelsFormat::Greyscale, "Expecting palette-index iamge format");
	Soy::Assert( Palette.GetFormat()==SoyPixelsFormat::RGB, "Expecting palette to be RGB");
	auto width = size_cast<uint16>( Image.GetWidth() );
	auto height = size_cast<uint16>( Image.GetHeight() );
	
    // graphics control extension
    Writer.fputc(0x21);
    Writer.fputc(0xf9);
    Writer.fputc(0x04);
    Writer.fputc(0x05); // leave prev frame in place, this frame has transparency
    Writer.fputc(delay & 0xff);
    Writer.fputc((delay >> 8) & 0xff);
    Writer.fputc(TransparentIndex); // transparent color index
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
	
	//	pallette size needs to be power2 aligned
	uint32 PaddedPaletteSize = size_cast<uint32>( Palette.GetWidth() );
	PaddedPaletteSize = isPowerOfTwo( PaddedPaletteSize ) ? PaddedPaletteSize : GetNextPowerOfTwo( PaddedPaletteSize );
	
    Writer.fputc(0x80 + GetBitIndex(PaddedPaletteSize) - 1); // local color table present, 2 ^ bitDepth entries
    GifWritePalette( Palette, PaddedPaletteSize, Writer );
    
	auto minCodeSize = size_cast<uint8>( GetBitIndex(PaddedPaletteSize) );
	const uint32_t clearCode = size_cast<const uint32_t>( PaddedPaletteSize );
	
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
bool GifBegin( GifWriter& writer, uint16 width, uint16 height, uint16 delay, int32_t bitDepth = 8, bool dither = false )
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
