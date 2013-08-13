// ===========================================================================
//	ExrFormat.r				Part of OpenEXR
// ===========================================================================


#include "PIGeneral.r"



//-------------------------------------------------------------------------------
//	PiPL resource
//-------------------------------------------------------------------------------

resource 'PiPL' (16000, "OpenEXR PiPL", purgeable)
{
    {
		Kind 				{ ImageFormat },
		Name 				{ "OpenEXR" },
		Version 			{ (latestFormatVersion << 16) | latestFormatSubVersion },		

#if MSWindows
		CodeWin32X86       { "PluginMain" },
#elif TARGET_API_MAC_CARBON
		CodeCarbonPowerPC	{ 0, 0, "" },
#else
        CodePowerPC 		{ 0, 0, "" },
#endif
		
		FmtFileType 		{ 'EXR ', '8BIM' },
		ReadTypes 			{ { 'EXR ', '    ' } },
		FilteredTypes 		{ { 'EXR ', '    ' } },
		ReadExtensions 		{ { 'exr ' } },
		WriteExtensions 	{ { 'exr ' } },
		FilteredExtensions 	{ { 'exr ' } },
		FormatMaxSize		{ { 32767, 32767 } },

		
		// this is to make us available when saving a 16-bit image  

		EnableInfo          
		{ 
			"in (PSHOP_ImageMode, RGBMode, RGB48Mode)"		
		},      


		// this is apparently just for backwards compatability

		SupportedModes
		{
			noBitmap,
			noGrayScale,
			noIndexedColor,
			doesSupportRGBColor,	// yes
			noCMYKColor,
			noHSLColor,
			noHSBColor,
			noMultichannel,
			noDuotone,
			noLABColor
#if !MSWindows			
			,
			noGray16,
			doesSupportRGB48,		// yes
			noLab48,
			noCMYK64,
			noDeepMultichannel,
			noDuotone16
#endif
		},

		FormatFlags 
		{
			fmtDoesNotSaveImageResources, 
			fmtCanRead, 
			fmtCanWrite, 
			fmtCanWriteIfRead, 
			fmtCannotWriteTransparency
#if MSWindows			
			,
			fmtCannotCreateThumbnail
#endif
		},	
		
		FormatMaxChannels 
		{ 
			{
				1, 
				4, 
				4, 
				4, 
				4, 
				4, 
				4, 
				4, 
				4, 
				4, 
				4, 
				4
			}
		}
	}
};



