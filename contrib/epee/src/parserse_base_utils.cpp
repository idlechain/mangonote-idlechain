// Copyright (c) 2006-2013, Andrey N. Sabelnikov, www.sabelnikov.net
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
// * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
// * Neither the name of the Andrey N. Sabelnikov nor the
// names of its contributors may be used to endorse or promote products
// derived from this software without specific prior written permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER  BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
// ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
// 

#include "storages/parserse_base_utils.h"

#include "misc_log_ex.h"
#include <boost/utility/string_ref.hpp>
#include <algorithm>

#undef MANGONOTE_DEFAULT_LOG_CATEGORY
#define MANGONOTE_DEFAULT_LOG_CATEGORY "serialization"

namespace epee 
{
namespace misc_utils
{
  namespace parse
  {
    std::string transform_to_escape_sequence(const std::string& src)
    {
      static const char escaped[] = "\b\f\n\r\t\v\"\\/";
      std::string::const_iterator it = std::find_first_of(src.begin(), src.end(), escaped, escaped + sizeof(escaped));
      if (it == src.end())
        return src;

      std::string res;
      res.reserve(2 * src.size());
      res.assign(src.begin(), it);
      for(; it!=src.end(); ++it)
      {
        switch(*it)
        {
        case '\b':  //Backspace (ascii code 08)
          res+="\\b"; break;
        case '\f':  //Form feed (ascii code 0C)
          res+="\\f"; break;
        case '\n':  //New line
          res+="\\n"; break;
        case '\r':  //Carriage return
          res+="\\r"; break;
        case '\t':  //Tab
          res+="\\t"; break;
        case '\v':  //Vertical tab
          res+="\\v"; break;
        //case '\'':  //Apostrophe or single quote
        //  res+="\\'"; break;
        case '"':  //Double quote
          res+="\\\""; break;
        case '\\':  //Backslash caracter
          res+="\\\\"; break;
        case '/':  //Backslash caracter
          res+="\\/"; break;
        default:
          res.push_back(*it);
        }
      }
      return res;
    }
    /*
      
      \b  Backspace (ascii code 08)
      \f  Form feed (ascii code 0C)
      \n  New line
      \r  Carriage return
      \t  Tab
      \v  Vertical tab
      \'  Apostrophe or single quote
      \"  Double quote
      \\  Backslash character

      */
      void match_string2(std::string::const_iterator& star_end_string, std::string::const_iterator buf_end, std::string& val)
      {
        bool escape_mode = false;
        std::string::const_iterator it = star_end_string;
        ++it;
        std::string::const_iterator fi = it;
        while (fi != buf_end && ((lut[(uint8_t)*fi] & 32)) == 0)
          ++fi;
        val.assign(it, fi);
        it = fi;
        for(;it != buf_end;++it)
        {
          if(escape_mode/*prev_ch == '\\'*/)
          {
            switch(*it)
            {
            case 'b':  //Backspace (ascii code 08)
              val.push_back(0x08);break;
            case 'f':  //Form feed (ascii code 0C)
              val.push_back(0x0C);break;
            case 'n':  //New line
              val.push_back('\n');break;
            case 'r':  //Carriage return
              val.push_back('\r');break;
            case 't':  //Tab
              val.push_back('\t');break;
            case 'v':  //Vertical tab
              val.push_back('\v');break;
            case '\'':  //Apostrophe or single quote
              val.push_back('\'');break;
            case '"':  //Double quote
              val.push_back('"');break;
            case '\\':  //Backslash character
              val.push_back('\\');break;
            case '/':  //Slash character
              val.push_back('/');break;
            case 'u':  //Unicode code point
              if (buf_end - it < 4)
              {
                ASSERT_MES_AND_THROW("Invalid Unicode escape sequence");
              }
              else
              {
                uint32_t dst = 0;
                for (int i = 0; i < 4; ++i)
                {
                  const unsigned char tmp = isx[(unsigned char)*++it];
                  CHECK_AND_ASSERT_THROW_MES(tmp != 0xff, "Bad Unicode encoding");
                  dst = dst << 4 | tmp;
                }
                // encode as UTF-8
                if (dst <= 0x7f)
                {
                  val.push_back(dst);
                }
                else if (dst <= 0x7ff)
                {
                  val.push_back(0xc0 | (dst >> 6));
                  val.push_back(0x80 | (dst & 0x3f));
                }
                else if (dst <= 0xffff)
                {
                  val.push_back(0xe0 | (dst >> 12));
                  val.push_back(0x80 | ((dst >> 6) & 0x3f));
                  val.push_back(0x80 | (dst & 0x3f));
                }
                else
                {
                  ASSERT_MES_AND_THROW("Unicode code point is out or range");
                }
              }
              break;
            default:
              val.push_back(*it);
              LOG_PRINT_L0("Unknown escape sequence :\"\\" << *it << "\"");
            }
            escape_mode = false;
          }else if(*it == '"')
          {
            star_end_string = it;
            return;
          }else if(*it == '\\')
          {
            escape_mode = true;
          }          
          else
          {
            val.push_back(*it); 
          }
        }
        ASSERT_MES_AND_THROW("Failed to match string in json entry: " << std::string(star_end_string, buf_end));
      }
      void match_number2(std::string::const_iterator& star_end_string, std::string::const_iterator buf_end, boost::string_ref& val, bool& is_float_val, bool& is_signed_val)
      {
        val.clear();
        uint8_t float_flag = 0;
        is_signed_val = false;
        size_t chars = 0;
        std::string::const_iterator it = star_end_string;
        if (it != buf_end && *it == '-')
        {
          is_signed_val = true;
          ++chars;
          ++it;
        }
        for(;it != buf_end;++it)
        {
          const uint8_t flags = lut[(uint8_t)*it];
          if (flags & 16)
          {
            float_flag |= flags;
            ++chars;
          }
          else
          {
            val = boost::string_ref(&*star_end_string, chars);
            if(val.size())
            {
              star_end_string = --it;
              is_float_val = !!(float_flag & 2);
              return;
            }
            else 
              ASSERT_MES_AND_THROW("wrong number in json entry: " << std::string(star_end_string, buf_end));
          }
        }
        ASSERT_MES_AND_THROW("wrong number in json entry: " << std::string(star_end_string, buf_end));
      }
      void match_word2(std::string::const_iterator& star_end_string, std::string::const_iterator buf_end, boost::string_ref& val)
      {
        val.clear();

        for(std::string::const_iterator it = star_end_string;it != buf_end;++it)
        {
          if (!(lut[(uint8_t)*it] & 4))
          {
            val = boost::string_ref(&*star_end_string, std::distance(star_end_string, it));
            if(val.size())
            {
              star_end_string = --it;
              return;
            }else 
              ASSERT_MES_AND_THROW("failed to match word number in json entry: " << std::string(star_end_string, buf_end));
          }
        }
        ASSERT_MES_AND_THROW("failed to match word number in json entry: " << std::string(star_end_string, buf_end));
      }
  }
}
}
