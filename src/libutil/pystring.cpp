///////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2008, Sony Pictures Imageworks
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
// 
// Redistributions of source code must retain the above copyright notice,
// this list of conditions and the following disclaimer.
// Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
// Neither the name of the organization Sony Pictures Imageworks nor the
// names of its contributors
// may be used to endorse or promote products derived from this software
// without specific prior written permission.
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS
// IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
// TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
// PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER
// OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
///////////////////////////////////////////////////////////////////////////////

#include <ctype.h>
#include <string.h>
#include <iostream>
#include <algorithm>
#include "pystring.h"

OIIO_NAMESPACE_ENTER
{

namespace pystring
{

	namespace {

		//////////////////////////////////////////////////////////////////////////////////////////////
		/// why doesn't the std::reverse work?
		///
		void reverse_strings( std::vector< std::string > & result)
		{
			for (std::vector< std::string >::size_type i = 0; i < result.size() / 2; i++ )
			{
				std::swap(result[i], result[result.size() - 1 - i]);
			}
		}

		//////////////////////////////////////////////////////////////////////////////////////////////
		///
		///
		void split_whitespace( const std::string & str, std::vector< std::string > & result, int maxsplit )
		{
			std::string::size_type i, j, len = str.size();
			for (i = j = 0; i < len; )
			{

				while ( i < len && ::isspace( str[i] ) ) i++;
				j = i;

				while ( i < len && ! ::isspace( str[i]) ) i++;



				if (j < i)
				{
					if ( maxsplit-- <= 0 ) break;

					result.push_back( str.substr( j, i - j ));

					while ( i < len && ::isspace( str[i])) i++;
					j = i;
				}
			}
			if (j < len)
			{
				result.push_back( str.substr( j, len - j ));
			}
		}


		//////////////////////////////////////////////////////////////////////////////////////////////
		///
		///
		void rsplit_whitespace( const std::string & str, std::vector< std::string > & result, int maxsplit )
		{
			std::string::size_type len = str.size();
			std::string::size_type i, j;
			for (i = j = len; i > 0; )
			{

				while ( i > 0 && ::isspace( str[i - 1] ) ) i--;
				j = i;

				while ( i > 0 && ! ::isspace( str[i - 1]) ) i--;



				if (j > i)
				{
					if ( maxsplit-- <= 0 ) break;

					result.push_back( str.substr( i, j - i ));

					while ( i > 0 && ::isspace( str[i - 1])) i--;
					j = i;
				}
			}
			if (j > 0)
			{
				result.push_back( str.substr( 0, j ));
			}
			//std::reverse( result, result.begin(), result.end() );
			reverse_strings( result );
		}

	} //anonymous namespace


    //////////////////////////////////////////////////////////////////////////////////////////////
    ///
    ///
    void split( const std::string & str, std::vector< std::string > & result, const std::string & sep, int maxsplit )
    {
        result.clear();
        
        if ( maxsplit < 0 ) maxsplit = MAX_32BIT_INT;//result.max_size();
        
        
        if ( sep.size() == 0 )
        {
            split_whitespace( str, result, maxsplit );
            return;
        }
        
        std::string::size_type i,j, len = str.size(), n = sep.size();
        
        i = j = 0;
        
        while ( i+n <= len )
        {
            if ( str[i] == sep[0] && str.substr( i, n ) == sep )
            {
                if ( maxsplit-- <= 0 ) break;
            
                result.push_back( str.substr( j, i - j ) );
                i = j = i + n;
            }
            else
            {
                i++;
            }
        }
        
        result.push_back( str.substr( j, len-j ) );
    }

    //////////////////////////////////////////////////////////////////////////////////////////////
    ///
    ///
    void rsplit( const std::string & str, std::vector< std::string > & result, const std::string & sep, int maxsplit )
    {
        if ( maxsplit < 0 )
        {
            split( str, result, sep, 0 );
            return;
        }

        result.clear();

        if ( sep.size() == 0 )
        {
            rsplit_whitespace( str, result, maxsplit );
            return;
        }

        std::string::size_type i,j, len = str.size(), n = sep.size();

        i = j = len;

        while ( i > n )
        {
            if ( str[i - 1] == sep[n - 1] && str.substr( i - n, n ) == sep )
            {
                if ( maxsplit-- <= 0 ) break;

                result.push_back( str.substr( i, j - i ) );
                i = j = i - n;
            }
            else
            {
                i--;
            }
        }

        result.push_back( str.substr( 0, j ) );
        reverse_strings( result );
    }

    //////////////////////////////////////////////////////////////////////////////////////////////
    ///
    ///
    #define LEFTSTRIP 0
    #define RIGHTSTRIP 1
    #define BOTHSTRIP 2
    
    //////////////////////////////////////////////////////////////////////////////////////////////
    ///
    ///
    bool __substrcmp( const std::string & str, const std::string & str2, std::string::size_type pos )
    {
        std::string::size_type len = str.size(), len2 = str2.size();
        if ( pos + len2 > len ) 
        {
            return false;
        }
        
        for ( std::string::size_type i = 0; i < len2; ++i )
        {
            
            if ( str[pos + i] != str2[i] )
            {
                return false;
            }
        }
        
        return true;
    }
    
    //////////////////////////////////////////////////////////////////////////////////////////////
    ///
    ///
    std::string do_strip( const std::string & str, int striptype, const std::string & chars  )
    {
        std::string::size_type len = str.size(), i, j, charslen = chars.size();
        
        if ( charslen == 0 )
        {
            i = 0;
            if ( striptype != RIGHTSTRIP )
            {
                while ( i < len && ::isspace( str[i] ) )
                {
                    i++;
                }
            }
            
            j = len;
            if ( striptype != LEFTSTRIP )
            {
                do
                {
                    j--;
                }
                while (j >= i && ::isspace(str[j]));
                
                j++;
            }
        
        
        }
        else
        {
            const char * sep = chars.c_str();
            
            i = 0;
            if ( striptype != RIGHTSTRIP )
            {
                while ( i < len && memchr(sep, str[i], charslen) )
                {
                    i++;
                }
            }
            
            j = len;
            if (striptype != LEFTSTRIP)
            {
                do
                {
                    j--;
                }
                while (j >= i &&  memchr(sep, str[j], charslen)  );
                j++;
            }
            
            
        }
        
        if ( i == 0 && j == len )
        {
            return str;
        }
        else
        {
            return str.substr( i, j - i );
        }
    
    }

    //////////////////////////////////////////////////////////////////////////////////////////////
    ///
    ///
    void partition( const std::string & str, const std::string & sep, std::vector< std::string > & result )
    {
        result.resize(3);
        int index = find( str, sep );
        if ( index < 0 )
        {
            result[0] = str;
            result[1] = "";
            result[2] = "";
        }
        else
        {
            result[0] = str.substr( 0, index );
            result[1] = sep;
            result[2] = str.substr( index + sep.size(), str.size() );
        }
    }

    //////////////////////////////////////////////////////////////////////////////////////////////
    ///
    ///
    void rpartition( const std::string & str, const std::string & sep, std::vector< std::string > & result )
    {
        result.resize(3);
        int index = rfind( str, sep );
        if ( index < 0 )
        {
            result[0] = "";
            result[1] = "";
            result[2] = str;
        }
        else
        {
            result[0] = str.substr( 0, index );
            result[1] = sep;
            result[2] = str.substr( index + sep.size(), str.size() );
        }
    }
    
    //////////////////////////////////////////////////////////////////////////////////////////////
    ///
    ///
    std::string strip( const std::string & str, const std::string & chars )
    {
        return do_strip( str, BOTHSTRIP, chars );
    }
    
    //////////////////////////////////////////////////////////////////////////////////////////////
    ///
    ///
    std::string lstrip( const std::string & str, const std::string & chars )
    {
        return do_strip( str, LEFTSTRIP, chars );
    }
    
    //////////////////////////////////////////////////////////////////////////////////////////////
    ///
    ///
    std::string rstrip( const std::string & str, const std::string & chars )
    {
        return do_strip( str, RIGHTSTRIP, chars );
    }
    
    //////////////////////////////////////////////////////////////////////////////////////////////
    ///
    ///
    std::string join( const std::string & str, const std::vector< std::string > & seq )
    {
        std::vector< std::string >::size_type seqlen = seq.size(), i;
        
        if ( seqlen == 0 ) return "";
        if ( seqlen == 1 ) return seq[0];
        
        std::string result( seq[0] );
        
        for ( i = 1; i < seqlen; ++i )
        {
            result += str + seq[i];
            
        }
        
        
        return result;
    }
    
    
    
    //////////////////////////////////////////////////////////////////////////////////////////////
    ///
    ///
    int __adjustslicepos( std::string::size_type len, int pos )
    {
        int intlen = len, value;
         
        if ( pos < 0 )
        {
            value = intlen + pos;
        }
        else
        {
            value = pos;
        }
            
        if ( value < 0 )
        {
            value = 0;
        }
        
        else if ( value > ( int ) len )
        {   
            value = len;
        }
        
        return value;
        
    }
    
    
    
    //////////////////////////////////////////////////////////////////////////////////////////////
    ///
    ///
    bool startswith( const std::string & str, const std::string & prefix, int start, int end )
    {
        std::string::size_type startp, endp;
        
        startp = __adjustslicepos( str.size(), start );
        endp = __adjustslicepos( str.size(), end );
        
        if ( start > (int) str.size() ) return false;
        
        if ( endp - startp < prefix.size()   ) return false;
        return __substrcmp( str, prefix, startp );
        
    }
    
    //////////////////////////////////////////////////////////////////////////////////////////////
    ///
    ///
    bool endswith( const std::string & str, const std::string & suffix, int start, int end )
    {
        std::string::size_type startp, endp;
        
        startp = __adjustslicepos( str.size(), start );
        endp = __adjustslicepos( str.size(), end );
        
        int upper = endp;
        int lower = ( upper - suffix.size() ) > startp ? ( upper - suffix.size() ) : startp;
        
        if ( start > (int) str.size() ) return false;
        
        
        if ( upper - lower < ( int ) suffix.size() )
        {
            return false;
        }
        
        
        return __substrcmp(str, suffix, lower );
    }
    
    //////////////////////////////////////////////////////////////////////////////////////////////
    ///
    ///
  
    bool isalnum( const std::string & str )
    {
        std::string::size_type len = str.size(), i;
        if ( len == 0 ) return false;
        
        
        if( len == 1 )
        {
            return ::isalnum( str[0] );
        }
        
        for ( i = 0; i < len; ++i )
        {
            if ( !::isalnum( str[i] ) ) return false;
        }
        return true;
    }
    
    //////////////////////////////////////////////////////////////////////////////////////////////
    ///
    ///
    bool isalpha( const std::string & str )
    {
        std::string::size_type len = str.size(), i;
        if ( len == 0 ) return false;
        if( len == 1 ) return ::isalpha( (int) str[0] );
        
        for ( i = 0; i < len; ++i )
        {
           if ( !::isalpha( (int) str[i] ) ) return false;
        }
        return true;
    }
    
    //////////////////////////////////////////////////////////////////////////////////////////////
    ///
    ///
    bool isdigit( const std::string & str )
    {
        std::string::size_type len = str.size(), i;
        if ( len == 0 ) return false;
        if( len == 1 ) return ::isdigit( str[0] );
        
        for ( i = 0; i < len; ++i )
        {
           if ( ! ::isdigit( str[i] ) ) return false;
        }
        return true;
    }
    
    //////////////////////////////////////////////////////////////////////////////////////////////
    ///
    ///
    bool islower( const std::string & str )
    {
        std::string::size_type len = str.size(), i;
        if ( len == 0 ) return false;
        if( len == 1 ) return ::islower( str[0] );
        
        for ( i = 0; i < len; ++i )
        {
           if ( !::islower( str[i] ) ) return false;
        }
        return true;
    }
    
    //////////////////////////////////////////////////////////////////////////////////////////////
    ///
    ///
    bool isspace( const std::string & str )
    {
        std::string::size_type len = str.size(), i;
        if ( len == 0 ) return false;
        if( len == 1 ) return ::isspace( str[0] );
        
        for ( i = 0; i < len; ++i )
        {
           if ( !::isspace( str[i] ) ) return false;
        }
        return true;
    }
    
    //////////////////////////////////////////////////////////////////////////////////////////////
    ///
    ///
    bool istitle( const std::string & str )
    {   
        std::string::size_type len = str.size(), i;
        
        if ( len == 0 ) return false;
        if ( len == 1 ) return ::isupper( str[0] );
        
        bool cased = false, previous_is_cased = false;
        
        for ( i = 0; i < len; ++i )
        {
            if ( ::isupper( str[i] ) )
            {
                if ( previous_is_cased ) 
                {
                    return false;
                }
                
                previous_is_cased = true;
                cased = true;
            }
            else if ( ::islower( str[i] ) )
            {
                if (!previous_is_cased)
                {
                    return false;
                }
                
                previous_is_cased = true;
                cased = true;
            
            }
            else
            {
                previous_is_cased = false;
            }
        }
        
        return cased;
    }
    
    //////////////////////////////////////////////////////////////////////////////////////////////
    ///
    ///
    bool isupper( const std::string & str )
    {
        std::string::size_type len = str.size(), i;
        if ( len == 0 ) return false;
        if( len == 1 ) return ::isupper( str[0] );
        
        for ( i = 0; i < len; ++i )
        {
           if ( !::isupper( str[i] ) ) return false;
        }
        return true;
    }
    
    //////////////////////////////////////////////////////////////////////////////////////////////
    ///
    ///
    std::string capitalize( const std::string & str )
    {
        std::string s( str );
        std::string::size_type len = s.size(), i;
        
        if ( len > 0)
        {
            if (::islower(s[0])) s[0] = ::toupper( s[0] );
        }
        
        for ( i = 1; i < len; ++i )
        {
            if (::isupper(s[i])) s[i] = ::tolower( s[i] );
        }
        
        return s;
    }
    
    //////////////////////////////////////////////////////////////////////////////////////////////
    ///
    ///
    std::string lower( const std::string & str )
    {
        std::string s( str );
        std::string::size_type len = s.size(), i;
        
        for ( i = 0; i < len; ++i )
        {
            if ( ::isupper( s[i] ) ) s[i] = ::tolower( s[i] );
        }
        
        return s;
    }
    
    //////////////////////////////////////////////////////////////////////////////////////////////
    ///
    ///
    std::string upper( const std::string & str )
    {
        std::string s( str ) ;
        std::string::size_type len = s.size(), i;
        
        for ( i = 0; i < len; ++i )
        {
            if ( ::islower( s[i] ) ) s[i] = ::toupper( s[i] );
        }
        
        return s;
    }
    
    //////////////////////////////////////////////////////////////////////////////////////////////
    ///
    ///
    std::string swapcase( const std::string & str )
    {
        std::string s( str );
        std::string::size_type len = s.size(), i;
        
        for ( i = 0; i < len; ++i )
        {
            if ( ::islower( s[i] ) ) s[i] = ::toupper( s[i] );
            else if (::isupper( s[i] ) ) s[i] = ::tolower( s[i] );
        }
        
        return s;
    }
    
    //////////////////////////////////////////////////////////////////////////////////////////////
    ///
    ///
    std::string title( const std::string & str )
    {
        std::string s( str );
        std::string::size_type len = s.size(), i;
        bool previous_is_cased = false;
        
        for ( i = 0; i < len; ++i )
        {
            int c = s[i];
            if ( ::islower(c) )
            {
                if ( !previous_is_cased )
                {
                    s[i] = ::toupper(c);
                }
                previous_is_cased = true;
            }
            else if ( ::isupper(c) )
            {
                if ( previous_is_cased )
                {
                    s[i] = ::tolower(c);
                }
                previous_is_cased = true;
            }
            else
            {
                previous_is_cased = false;
            }
        }
        
        return s;
    }
    
    //////////////////////////////////////////////////////////////////////////////////////////////
    ///
    ///
    std::string translate( const std::string & str, const std::string & table, const std::string & deletechars )
    {
        std::string s;
        std::string::size_type len = str.size(), dellen = deletechars.size();
        
        if ( table.size() != 256 ) 
        {
            //raise exception instead
            return str;
        }
        
        //if nothing is deleted, use faster code
        if ( dellen == 0 ) 
        {
            s = str;
            for ( std::string::size_type i = 0; i < len; ++i )
            {
                s[i] = table[ s[i] ];
            }
            return s;
        }
        
        
        int trans_table[256];
        for ( int i = 0; i < 256; i++)
        {
            trans_table[i] = table[i];
        }
        
        for ( std::string::size_type i = 0; i < dellen; i++)
        {
            trans_table[(int) deletechars[i] ] = -1;
        }
        
        for ( std::string::size_type i = 0; i < len; ++i )
        {
            if ( trans_table[ (int) str[i] ] != -1 )
            {
                s += table[ s[i] ];
            }
        }
        
        return s;
    
    }
    
    
    //////////////////////////////////////////////////////////////////////////////////////////////
    ///
    ///
    std::string zfill( const std::string & str, int width )
    {
        std::string::size_type len = str.size();
        
        if ( ( int ) len >= width )
        {
            return str;
        }
        
        std::string s( str );
         
        int fill = width - len;
        
        s = std::string( fill, '0' ) + s;
        
        
        if ( s[fill] == '+' || s[fill] == '-' )
        {
            s[0] = s[fill];
            s[fill] = '0';
        }
        
        return s;
    
    }
    
    //////////////////////////////////////////////////////////////////////////////////////////////
    ///
    ///
    std::string ljust( const std::string & str, int width )
    {
        std::string::size_type len = str.size();
        if ( (( int ) len ) >= width ) return str;
        return str + std::string( width - len, ' ' );
    }
    
    //////////////////////////////////////////////////////////////////////////////////////////////
    ///
    ///
    std::string rjust( const std::string & str, int width )
    {
        std::string::size_type len = str.size();
        if ( (( int ) len ) >= width ) return str;
        return std::string( width - len, ' ' ) + str;
    }
    
    //////////////////////////////////////////////////////////////////////////////////////////////
    ///
    ///
    std::string center( const std::string & str, int width )
    {
        std::string::size_type len = str.size();
        int marg, left;
        
        if ( (( int ) len ) >= width ) return str;
    
        marg = width - len; 
        left = marg / 2 + (marg & width & 1);
        
        return std::string( left, ' ' ) + str + std::string( marg - left, ' ' );
        
    }
    
    //////////////////////////////////////////////////////////////////////////////////////////////
    ///
    ///
    int find( const std::string & str, const std::string & sub, int start, int end  )
    {
        std::string::size_type startp, endp;
        
        startp = __adjustslicepos( str.size(), start );
        endp = __adjustslicepos( str.size(), end );
        
        std::string::size_type result;
        
        result = str.find( sub, startp );
        
        if( result == std::string::npos || result >= endp)
        {
            return -1;
        }
        
        return result;
    }
    
    //////////////////////////////////////////////////////////////////////////////////////////////
    ///
    ///
    int index( const std::string & str, const std::string & sub, int start, int end  )
    {
        return find( str, sub, start, end );
    }
    
    //////////////////////////////////////////////////////////////////////////////////////////////
    ///
    ///
    int rfind( const std::string & str, const std::string & sub, int start, int end )
    {
        std::string::size_type startp, endp;
        
        startp = __adjustslicepos( str.size(), start );
        endp = __adjustslicepos( str.size(), end );
        
        std::string::size_type result;
        
        result = str.rfind( sub, endp );
        
        if( result == std::string::npos || result < startp ) return -1;
        return result;
        
    }
    
    //////////////////////////////////////////////////////////////////////////////////////////////
    ///
    ///
    int rindex( const std::string & str, const std::string & sub, int start, int end )
    {
        return rfind( str, sub, start, end );
    }
    
    //////////////////////////////////////////////////////////////////////////////////////////////
    ///
    ///
    std::string expandtabs( const std::string & str, int tabsize )
    {
        std::string s( str );
        
        std::string::size_type len = str.size(), i = 0;
        int offset = 0;
        
        int j = 0;
        
        for ( i = 0; i < len; ++i )
        {
            if ( str[i] == '\t' )
            {
                
                if ( tabsize > 0 )
                {
                    int fillsize = tabsize - (j % tabsize);
                    j += fillsize;
                    s.replace( i + offset, 1, std::string( fillsize, ' ' ));
                    offset += fillsize - 1;
                }
                else
                {
                    s.replace( i + offset, 1, "" );
                    offset -= 1;
                }
            
            }
            else
            {
                j++;
                
                if (str[i] == '\n' || str[i] == '\r')
                {
                    j = 0;
                }
            }
        }
        
        return s;
    }
    
    //////////////////////////////////////////////////////////////////////////////////////////////
    ///
    ///
    int count( const std::string & str, const std::string & substr, int start, int end )
    {
        int nummatches = 0;
        int cursor = start;
        
        while ( 1 )
        {
            cursor = find( str, substr, cursor, end );
            
            if ( cursor < 0 ) break;
            
            cursor += substr.size();
            nummatches += 1;
        }
       
        return nummatches;
        
    
    }
    
    //////////////////////////////////////////////////////////////////////////////////////////////
    ///
    ///
    std::string replace( const std::string & str, const std::string & oldstr, const std::string & newstr, int count )
    {
        int sofar = 0;
        int cursor = 0;
        std::string s( str );
        
        std::string::size_type oldlen = oldstr.size(), newlen = newstr.size();
        
        while ( ( cursor = find( s, oldstr, cursor ) ) != -1 )
        {
            if ( count > -1 && sofar >= count )
            {
                break;
            }
        
            s.replace( cursor, oldlen, newstr );
            
            cursor += newlen;
            ++sofar;
        }
        
        return s;
        
    }
    
    //////////////////////////////////////////////////////////////////////////////////////////////
    ///
    ///
    void splitlines(  const std::string & str, std::vector< std::string > & result, bool keepends )
    {
        result.clear();
        std::string::size_type len = str.size(), i, j, eol;
        
         for (i = j = 0; i < len; )
         {
            while (i < len && str[i] != '\n' && str[i] != '\r') i++;
         
            eol = i;
            if (i < len)
            {
                if (str[i] == '\r' && i + 1 < len && str[i+1] == '\n')
                {
                    i += 2;
                }
                else
                {
                    i++;
                }
                if (keepends)
                eol = i;
                
            }
            
            result.push_back( str.substr( j, eol - j ) );
            j = i;
         
        }
         
        if (j < len)
        {
            result.push_back( str.substr( j, len - j ) );
        }
        
    }
    
    //////////////////////////////////////////////////////////////////////////////////////////////
    ///
    ///
    std::string slice( const std::string & str, int start, int end )
    {
        std::string::size_type startp, endp;
        
        startp = __adjustslicepos( str.size(), start );
        endp = __adjustslicepos( str.size(), end );
        
        if ( startp >= endp ) return "";
        
        return str.substr( startp, endp - startp );
    }
    
    
    
}//namespace pystring

}
OIIO_NAMESPACE_EXIT
