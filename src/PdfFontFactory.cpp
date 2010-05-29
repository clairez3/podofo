/***************************************************************************
 *   Copyright (C) 2007 by Dominik Seichter                                *
 *   domseichter@web.de                                                    *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU Library General Public License as       *
 *   published by the Free Software Foundation; either version 2 of the    *
 *   License, or (at your option) any later version.                       *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU Library General Public     *
 *   License along with this program; if not, write to the                 *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include "PdfFontFactory.h"

#include "PdfArray.h"
#include "PdfDictionary.h"
#include "PdfEncoding.h"
#include "PdfEncodingFactory.h"
#include "PdfFont.h"
#include "PdfFontCID.h"
#include "PdfFontMetrics.h"
#include "PdfFontMetricsBase14.h"
#include "PdfFontMetricsObject.h"
#include "PdfFontType1.h"
#include "PdfFontType1Base14.h"
#include "PdfFontTrueType.h"
#include "PdfDefinesPrivate.h"
#include "PdfFontFactoryBase14Data.h"

namespace PoDoFo {

PdfFont* PdfFontFactory::CreateFontObject( PdfFontMetrics* pMetrics, int nFlags, 
                                           const PdfEncoding* const pEncoding,
                                           PdfVecObjects* pParent )
{
    PdfFont*     pFont  = NULL;
    EPdfFontType eType  = pMetrics->GetFontType();
    bool         bEmbed = nFlags & ePdfFont_Embedded;

    try
    { 
        pFont = PdfFontFactory::CreateFontForType( eType, pMetrics, pEncoding, bEmbed, pParent );
        
        if( pFont ) 
        {
            pFont->SetBold( nFlags & ePdfFont_Bold ? true : false );
            pFont->SetItalic( nFlags & ePdfFont_Italic ? true : false );
        }
        else
        {
            // something went wrong, so we have to delete
            // the font metrics
            delete pMetrics;
            // make sure this will be done before the catch block
            // as the encoding might be deleted already
            // afterwars, but we cannot set the pointer to NULL
            if( pEncoding && pEncoding->IsAutoDelete() )
                delete pEncoding;
        }
    }
    catch( PdfError & e ) 
    {
        // we have to delete the pMetrics object in case of error 
        if( pFont ) 
        {
            // The font will delete encoding and metrics
            delete pFont;
            pFont = NULL;
        }
        else
        {
            // something went wrong, so we have to delete
            // the font metrics (and if auto-delete, also the encoding)
            delete pMetrics;
            pMetrics = NULL;

            if( pEncoding && pEncoding->IsAutoDelete() )
                delete pEncoding;
        }

        e.AddToCallstack( __FILE__, __LINE__, "Font creation failed." );
        throw e;
        
    }
    
    return pFont;
}

PdfFont* PdfFontFactory::CreateFontForType( EPdfFontType eType, PdfFontMetrics* pMetrics, 
                                            const PdfEncoding* const pEncoding, 
                                            bool bEmbed, PdfVecObjects* pParent )
{
    PdfFont* pFont = NULL;

    if( pEncoding->IsSingleByteEncoding() ) 
    {
        switch( eType ) 
        {
            case ePdfFontType_TrueType:
                // Peter Petrov 30 April 2008 - added bEmbed parameter
                pFont = new PdfFontTrueType( pMetrics, pEncoding, pParent, bEmbed );
                break;
                
            case ePdfFontType_Type1Pfa:
            case ePdfFontType_Type1Pfb:
                pFont = new PdfFontType1( pMetrics, pEncoding, pParent, bEmbed );
                break;
                
            case ePdfFontType_Unknown:
            case ePdfFontType_Type1Base14:
            default:
                PdfError::LogMessage( eLogSeverity_Error, "The font format is unknown. Fontname: %s Filename: %s\n", 
                                      (pMetrics->GetFontname() ? pMetrics->GetFontname() : "<unknown>"),
                                      (pMetrics->GetFilename() ? pMetrics->GetFilename() : "<unknown>") );
        }
    }
    else
    {
        switch( eType ) 
        {
            case ePdfFontType_TrueType:
                // Peter Petrov 30 April 2008 - added bEmbed parameter
                pFont = new PdfFontCID( pMetrics, pEncoding, pParent, bEmbed );
                break;
            case ePdfFontType_Type1Pfa:
            case ePdfFontType_Type1Pfb:
            case ePdfFontType_Type1Base14:
            case ePdfFontType_Unknown:
            default:
                PdfError::LogMessage( eLogSeverity_Error, 
                                      "The font format is unknown or no multibyte encoding defined. Fontname: %s Filename: %s\n", 
                                      (pMetrics->GetFontname() ? pMetrics->GetFontname() : "<unknown>"),
                                      (pMetrics->GetFilename() ? pMetrics->GetFilename() : "<unknown>") );
        }
    }

    return pFont;
}

PdfFont* PdfFontFactory::CreateFont( FT_Library* pLibrary, PdfObject* pObject )
{
    PdfFontMetrics* pMetrics    = NULL;
    PdfFont*        pFont       = NULL;
    PdfObject*      pDescriptor = NULL;
    PdfObject*      pEncoding   = NULL;

    if( pObject->GetDictionary().GetKey( PdfName::KeyType )->GetName() != PdfName("Font") )
    {
        PODOFO_RAISE_ERROR( ePdfError_InvalidDataType );
    }

    const PdfName & rSubType = pObject->GetDictionary().GetKey( PdfName::KeySubtype )->GetName();
    if( rSubType == PdfName("Type0") ) 
    {
        const PdfArray & descendant  = 
            pObject->GetDictionary().GetKey( "DescendantFonts" )->GetArray();
        PdfObject* pFontObject = pObject->GetOwner()->GetObject( descendant[0].GetReference() );

        pDescriptor = pFontObject->GetIndirectKey( "FontDescriptor" );
        pEncoding   = pObject->GetIndirectKey( "Encoding" );

        const PdfEncoding* const pPdfEncoding = 
            PdfEncodingFactory::CreateEncoding( pEncoding );

        pMetrics    = new PdfFontMetricsObject( pDescriptor, pPdfEncoding );
        pFont       = new PdfFontCID( pMetrics, pPdfEncoding, pObject, false );
    }
    else if( rSubType == PdfName("Type1") ) 
    {
        // TODO: Old documents do not have a FontDescriptor for 
        //       the 14 standard fonts. This suggestions is 
        //       deprecated now, but give us problems with old documents.
        pDescriptor = pObject->GetIndirectKey( "FontDescriptor" );
        pEncoding   = pObject->GetIndirectKey( "Encoding" );

        const PdfEncoding* const pPdfEncoding = 
            PdfEncodingFactory::CreateEncoding( pEncoding );

        pMetrics    = new PdfFontMetricsObject( pDescriptor, pPdfEncoding );
        pFont       = new PdfFontType1( pMetrics, pPdfEncoding, pObject );
    }
    else if( rSubType == PdfName("TrueType") ) 
    {
        pDescriptor = pObject->GetIndirectKey( "FontDescriptor" );
        pEncoding   = pObject->GetIndirectKey( "Encoding" );

        const PdfEncoding* const pPdfEncoding = 
            PdfEncodingFactory::CreateEncoding( pEncoding );

        pMetrics    = new PdfFontMetricsObject( pDescriptor, pPdfEncoding );
        pFont       = new PdfFontTrueType( pMetrics, pPdfEncoding, pObject );
    }

    return pFont;
}

EPdfFontType PdfFontFactory::GetFontType( const char* pszFilename )
{
    EPdfFontType eFontType = ePdfFontType_Unknown;

    // We check by file extension right now
    // which is not quite correct, but still better than before

    if( pszFilename && strlen( pszFilename ) > 3 )
    {
        const char* pszExtension = pszFilename + strlen( pszFilename ) - 3;
        if( PoDoFo::compat::strncasecmp( pszExtension, "ttf", 3 ) == 0 )
            eFontType = ePdfFontType_TrueType;
        else if( PoDoFo::compat::strncasecmp( pszExtension, "pfa", 3 ) == 0 )
            eFontType = ePdfFontType_Type1Pfa;
        else if( PoDoFo::compat::strncasecmp( pszExtension, "pfb", 3 ) == 0 )
            eFontType = ePdfFontType_Type1Pfb;
    }

    return eFontType;
}


PdfFontMetricsBase14*
PODOFO_Base14FontDef_FindBuiltinData(const char  *font_name)
{
    unsigned int i = 0;
	bool found = false;
    while (PODOFO_BUILTIN_FONTS[i].font_name) {
        if (strcmp(PODOFO_BUILTIN_FONTS[i].font_name, font_name) == 0) // kaushik  : HPDFStrcmp changed to strcmp
		{
			found = true;
			break;
		}

        i++;
    }

	return found ? &PODOFO_BUILTIN_FONTS[i] : NULL;
}

PdfFont* CreateBase14Font(const char* pszFontName, const PdfEncoding * const pEncoding,PdfVecObjects *pvecObjects)
{
	return new PdfFontType1Base14(PODOFO_Base14FontDef_FindBuiltinData(pszFontName), pEncoding, pvecObjects);
}

};
