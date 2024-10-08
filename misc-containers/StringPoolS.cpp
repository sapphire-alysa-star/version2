/*************************  StringPoolS.cpp   *********************************
* Author:        Agner Fog
* Date created:  2008-06-12
* Last modified: 2011-07-29
* Description:
* Defines memory pool for storing ASCII strings or UTF-8 encoded strings of
* arbitrary length.
*
* The latest version of this file is available at:
* www.agner.org/optimize/cppexamples.zip
* (c) 2008-2011 GNU General Public License www.gnu.org/copyleft/gpl.html
*******************************************************************************
*
* This string pool is a useful replacement for string classes such as 'string' 
* or 'CString'. It stores all strings in the same allocated memory block
* rather than allocating a new memory block for each string. This is 
* faster and causes less fragmentation of memory.
*
* StringPoolS allows strings of any length to be stored and manipulated
* without the risk of writing outside the bounds of a buffer, that the old
* C-style strings have.
*
* StringPoolS does not remember the length of each string. The length is
* measured every time a string is accessed. This can save memory space if
* there are many short strings and each string is accessed few times.
* Alternatively, StringPoolL stores the length of each string. This is faster
* if there are many long strings and the strings are manipulated often.
*
* StringPoolS is not thread safe if shared between threads. If your program 
* handles strings in multiple threads then each thread must have its own 
* private StringPoolS.
*
* StringPoolS may optionally use the function library asmlib for fast string 
* handling, especially fast string searching. Define USE_ASMLIB to use this.
* The asmlib library is available at (www.agner.org/optimize/asmlib.zip) 
*
* Each string is identified by an integer index. For example:
* StringPoolS strings;
* strings[20] = "Hello world";
* strings[20].Write(stdout);
*
* This example will store the text "Hello world" as string number 20, and 
* print it to the standard output. All unused string numbers below the 
* highest used number are set to empty strings. In the above example, 
* strings[3] contains an empty string, while an attempt to read strings[21]
* would generate an error message. It is OK to have unused strings as long
* as the number is not excessive. An unused string uses 4 bytes of memory,
* while a used string uses 5 bytes plus its length. For example, a program
* that uses string numbers starting at 100 in one part of the program and
* string numbers starting at 200 in another part of the program would not
* be overly wasteful, but a program with a million unused strings would 
* waste four megabytes of memory.
*
* StringPoolS allocates a memory block of at least size AllocateSpace1  
* = 4 kilobytes when the first string is stored. You may change this value.
* If the memory block is filled up, it will allocate a new memory block of
* more than the double size and move all strings to the new block. Garbage
* collection takes place only when a new memory block is allocated.
*
* Note that you should never store a pointer to a string in the string pool
* because the pointer will become invalid in case the assignment or
* modification of another string causes the memory to be re-allocated.
* All strings should be identified by their index, not by pointers. 
*
* Attempts to read a non-existing string or read beyond the end of a string
* will cause an error message to the standard error output. You may change
* the StringPoolS::Error function to produce a message box if the program has
* a graphical user interface.
*
* At the end of this file you will find a working example of how to use 
* StringPoolS and how to manipulate strings.
*
* The first part of this file containing declarations may be placed in a 
* header file. The second part containing function definitions should be in
* a .cpp file. The third part containing examples should be removed from your
* final application.
*
******************************************************************************/

/******************************************************************************
 1. Header part. Put this in a .h file:
******************************************************************************/

// Define USE_ASMLIB if you want to use the asmlib library for fast string handling
//#define  USE_ASMLIB               // Use asmlib library

#define  _CRT_SECURE_NO_WARNINGS  // Avoid warning for vsnprintf function in MS compiler
#include <memory.h>               // For memcpy and memset
#include <string.h>               // For strlen, strcmp, strchr
#include <stdlib.h>               // For exit in Error function
#include <stdarg.h>               // For va_list, va_start
#include <stdio.h>                // Needed for example only
//#include <varargs.h>            // Include varargs.h for va_list, va_start only if required by your system

#ifdef   USE_ASMLIB
#include "asmlib.h"               // Header file for asmlib library
#define  memcpy_  A_memcpy        // Use asmlib versions of string functions
#define  memset_  A_memset
#define  strlen_  A_strlen
#define  strcmp_  A_strcmp
#define  strstr_  A_strstr
#define  MEM_PADDING 15           // Padding of memory buffer
#else
#define  memcpy_  memcpy          // Use standard library versions of string functions
#define  memset_  memset
#define  strlen_  strlen
#define  strcmp_  strcmp
#define  strstr_  strstr
#define  MEM_PADDING 0
#endif

// Define pointer to zero-terminated string
typedef char const * PChar;

// Class definition for StringPoolS
class StringPoolS {
public:
   // Helper class StringElement. This class defines all the operators and 
   // functions we can apply to StringPoolSObject[i]:
   class StringElement {
   public:
      // Constructor:
      StringElement(StringPoolS & a, int i) : arr(a) {index = i;}; 
      // Get string length:
      int Len() {return strlen_(arr.Get(index));};
      // Format string using printf style formating:
      StringElement const & Printf(const char * format, ...);
      // Write string to file or stdout:
      int Write(FILE * f);
      // Search in string:
      int SearchForSubstring(PChar s);
      // Assign substring:
      StringElement const & SetToSubstring(PChar s, int start, int len);
      // Operator '=' assigns string:
      PChar operator = (PChar s) {arr.Set(index, s); return *this;};
      PChar operator = (StringElement const & s) {return *this = PChar(s);};
      // Operator '+=' concatenates strings:
      void operator += (const char * s) {arr.Concatenate(index, s);};
      // Automatic conversion to const char *
      operator PChar() const {return arr.Get(index);};
      // Operator '[]' gives access to a single character in a string:
      inline char & operator[] (int i);
   protected:
      StringPoolS & arr;               // Reference to the StringPoolS that created this object
      int index;                       // Index to string in StringPoolS
   };
   friend class StringElement;         // Friend class defining all operations we can do on StringPoolObject[i]

   // Members of class StringPoolS
   StringPoolS();                      // Constructor
   ~StringPoolS();                     // Destructor
   void Clear();                       // Erase all strings
   int GetNum() const {return Num;}    // Get number of strings
   StringElement operator[] (int i) {  // Operator '[]' gives access to a string in StringPoolS 
      return StringElement(*this, i);}
   void Set(int i, PChar s);           // Insert zero-terminated string. Used by StringElement 
   void Set(int i, PChar s, int Len);  // Insert non zero-terminated string. Used by StringElement 
   void Concatenate(int i, PChar s);   // Concatenate strings. Used by StringElement 
   void ReserveBuf(int newsize);       // Allocate memory for string space
   void ReserveNum(int newsize);       // Allocate memory for string indices

   // Define desired allocation sizes. You may change these values:
   enum DefineSizes {
      AllocateSpace1 = 4096,           // Minimum number of characters to allocate in string buffer 
      AllocateSpace2 = 1024,           // Minimum number of indices to allocate in offsets buffer
      FormatLength   = 1023,           // Maximum length of strings written with Printf
      MemPadding     = MEM_PADDING     // Memory buffer padding
   };
protected:
   char * Get(int i) const;            // Read string. Used only from StringElement 
   void Error(int message,int i) const;// Produce fatal error message.
private:
   char * Buffer;                      // Memory block containing strings
   char * OldBuffer;                   // Save old Buffer during copying of string
   unsigned int * Offsets;             // Memory block containing offsets to strings
   int BufferSize;                     // Size of buffer block
   int OffsetsSize;                    // Size of Offsets block
   int DataSize;                       // Used part of Buffer, including garbage
   int GarbageSize;                    // Total size of garbage in Buffer
   int Top;                            // Highest used Offset
   int Num;                            // Number of strings = highest used index in Offsets + 1
   char * Allocate(int i, int len);    // Make space for a new string
   StringPoolS(StringPoolS &){};       // Private copy constructor to prevent copying
   void operator = (StringPoolS &){};  // Private operator = to prevent copying
};


/******************************************************************************
Operators for comparing strings. You may include these if needed.
Replace strcmp by stricmp (Windows) or strcasecmp (Linux) if you want 
case-insensitive compares. Use A_stricmp in asmlib for fast compares that 
ignore the case for a-z, but not for other letters.
******************************************************************************/

inline bool operator == (StringPoolS::StringElement const & a, StringPoolS::StringElement const & b) {
   return strcmp_(a, b) == 0;
}
inline bool operator != (StringPoolS::StringElement const & a, StringPoolS::StringElement const & b) {
   return strcmp_(a, b) != 0;
}
inline bool operator  < (StringPoolS::StringElement const & a, StringPoolS::StringElement const & b) {
   return strcmp_(a, b) < 0;
}
inline bool operator  > (StringPoolS::StringElement const & a, StringPoolS::StringElement const & b) {
   return strcmp_(a, b) > 0;
}


/******************************************************************************
 2. Function definition part. Put this in a .cpp file:
******************************************************************************/

// Default constructor
StringPoolS::StringPoolS() {
   // Set everything to zero
   memset_(this, 0, sizeof(*this));
}


// Destructor
StringPoolS::~StringPoolS() {
   // Free allocated memory
   if (Buffer)    delete[] Buffer;
   if (OldBuffer) delete[] OldBuffer;
   if (Offsets)   delete[] Offsets;
   // Set everything to zero
   memset_(this, 0, sizeof(*this));
}


// Erase all strings
void StringPoolS::Clear() {
   // Set all offsets to zero
   if (Offsets) memset_(Offsets, 0, Num * sizeof(*Offsets));
   // Indicate that Buffer is empty, but do not deallocate
   DataSize = GarbageSize = 0;
}


// Insert string. Used by StringElement 
void StringPoolS::Set(int i, PChar s) {
   if (i < 0) Error(1, i);
   if (i >= Num) {
      // i is higher than any previous index
      Num = i + 1;
      if (i >= OffsetsSize) {
         // Make Offsets buffer bigger
         ReserveNum(i + 1);
      }
   }
   // Length of new string
   int Len = 0;
   if (s) Len = strlen_(s);
   if (Len == 0) {
      // Erase string
      if (Offsets[i]) {
         GarbageSize += strlen_(Buffer + Offsets[i]) + 1;
         Offsets[i] = 0;
      }
      return;
   }
   // Make space for string
   char * p = Allocate(i, Len);
   // Insert string
   memcpy_(p, s, Len+1);
   // Release OldBuffer if any
   if (OldBuffer) {
      delete[] OldBuffer;  OldBuffer = 0;
   }
}


// Insert non zero-terminated string. Used by StringElement
void StringPoolS::Set(int i, PChar s, int Len) {
   if (i < 0) Error(1, i);
   if (i >= Num) {
      // i is higher than any previous index
      Num = i + 1;
      if (i >= OffsetsSize) {
         // Make Offsets buffer bigger
         ReserveNum(i + 1);
      }
   }
   // Length of new string
   if (Len <= 0) {
      // Erase string
      if (Offsets[i]) {
         GarbageSize += strlen_(Buffer + Offsets[i]) + 1;
         Offsets[i] = 0;
      }
      return;
   }
   // Make space for string
   char * p = Allocate(i, Len);
   // Insert string
   memcpy_(p, s, Len);
   // Zero-terminate
   p[Len] = 0;
   // Release OldBuffer if any
   if (OldBuffer) {
      delete[] OldBuffer;  OldBuffer = 0;
   }
}


// Concatenate strings. Used by StringElement 
void StringPoolS::Concatenate(int i, PChar s) {
   int len1;                           // Length of first string   
   int len2 = 0;                       // Length of second string
   if (s) len2 = strlen_(s);
   if (len2 == 0) {
      // Second string is empty. Do nothing
      return;
   }
   if ((unsigned int)i >= (unsigned int)Num) Error(1, i); // Out of range
   if (Offsets[i] == 0) {
      // First string is empty. Just insert new string
      Set(i, s);
      return;
   }
   // Length of first string
   len1 = strlen_(Buffer + Offsets[i]);
   // Remember position of string1
   PChar string1 = Buffer + Offsets[i];
   // Make space for combined string
   char * p = Allocate(i, len1 + len2);
   // Copy strings
   memcpy_(p, string1, len1);
   memcpy_(p + len1, s, len2 + 1);
   // Release OldBuffer if any
   if (OldBuffer) {
      delete[] OldBuffer;  OldBuffer = 0;
   }
}


// Allocate new memory for string space and do garbage collection
void StringPoolS::ReserveBuf(int newsize) {   
   int i;                              // Loop counter
   int DataSize2 = 0;                  // Used part of buffer after garbage collection
   int Strlen;                         // Length of current string

   // Decide new size
   if (newsize < (DataSize - GarbageSize) * 2 + AllocateSpace1) {
       newsize = (DataSize - GarbageSize) * 2 + AllocateSpace1;
   }
   // Allocate new larger block
   char * Buffer2 = new char [newsize + 1 + MemPadding];
   if (Buffer2 == 0) Error(3, newsize + 1);// Allocation failed

   // Make empty string at offset 0. This will be used for all empty strings
   Buffer2[0] = 0;  DataSize2 = Top = 1;

   // Copy strings from old to new buffer
   if (Buffer) {
      // Loop through old indices
      for (i = 0; i < Num; i++) {
         if (Offsets[i]) {
            // Length of string
            Strlen = strlen_(Buffer + Offsets[i]);
            if (Strlen) {
               // String is not empty, copy it
               memcpy_ (Buffer2 + DataSize2, Buffer + Offsets[i], Strlen + 1);
               // Store new offset
               Offsets[i] = Top = DataSize2;
               // Offset to next
               DataSize2 += Strlen + 1;
            }
            else {
               // Empty string found
               Offsets[i] = 0;  GarbageSize++;
            }
         }
      }
      // OldBuffer should be empty here.
      // This check should not be necessary in single-thread applications:
      if (OldBuffer) {delete[] OldBuffer; OldBuffer = 0;}

      // Save old buffer, but don't delete it yet, because it might
      // contain a string being copied. Remember to delete Oldbuffer
      // after new string has been stored
      OldBuffer = Buffer;
   }

   // Save new buffer
   Buffer = Buffer2;  BufferSize = newsize + 1;

   // Save new DataSize
   DataSize = DataSize2;  GarbageSize = 0;
}


// Allocate memory for string indices
void StringPoolS::ReserveNum(int newsize) {

   // Only grow, not shrink:
   if (newsize <= OffsetsSize) return; 

   // Decide new size
   if (newsize < OffsetsSize * 2 + AllocateSpace2) {
       newsize = OffsetsSize * 2 + AllocateSpace2;
   }

   // Allocate new larger block
   unsigned int * Offsets2 = new unsigned int [newsize];
   if (Offsets2 == 0) Error(3, newsize); // Allocation failed

   // Copy indices to new block unless empty
   if (Offsets) memcpy_(Offsets2, Offsets, OffsetsSize * sizeof(*Offsets));

   // Set rest of new block to zero
   if (newsize > OffsetsSize) {
      memset_(Offsets2 + OffsetsSize, 0, (newsize - OffsetsSize) * sizeof(*Offsets2));
   }

   // Deallocate old block
   if (Offsets) delete[] Offsets;

   // Save pointer to new block
   Offsets = Offsets2;  OffsetsSize = newsize;
}


// Read string. Used only by StringElement 
char * StringPoolS::Get(int i) const {
   // Check that i is within range
   if ((unsigned int)i >= (unsigned int)Num) {
      // Out of range
      Error(1, i);      
   }
   // Return pointer
   return Buffer + Offsets[i];
}


// Produce fatal error message. Used internally and by StringElement.
// Note: If your program has a graphical user interface (GUI) then you
// must rewrite this function to produce a message box with the error message.
void StringPoolS::Error(int message, int i) const {
   // Define error texts
   static const char * ErrorTexts[] = {
      "Unknown error",                 // 0
      "Index out of range",            // 1
      "Going beyond end of string",    // 2
      "Memory allocation failed",      // 3
      "Formatted string too long"      // 4
   };
   // Number of texts in ErrorTexts
   const unsigned int NumErrorTexts = sizeof(ErrorTexts) / sizeof(*ErrorTexts);

   // check that index is within range
   if ((unsigned int)message >= NumErrorTexts) message = 0;

   // Replace this with your own error routine, possibly with a message box:
   fprintf(stderr, "\nStringPool error: %s (%i)\n", ErrorTexts[message], i);

   // Terminate execution
   exit(1);
}


// Make space for a new string. Used only internally
char * StringPoolS::Allocate(int i, int len) {
   
   // This is allready done by Set or Concatenate before calling Allocate:
   // if (i >= OffsetsSize) ReserveNum(i+1); 

   if (Offsets[i]) {
      // Index i allready has a string
      if (Offsets[i] == Top) {
         // This is last in Buffer. Can grow without overwriting other strings
         if (Top + len + 1 > BufferSize) {
            // Buffer not big enough
            ReserveBuf(Top + len + 1);
         }
         // Reserve size
         DataSize = Top + len + 1;
         // Return pointer. Offsets[i] unchanged
         return Buffer + Top;
      }
      // Length of old string
      int OldLen = strlen_(Buffer + Offsets[i]);
      if (OldLen >= len) {
         // New string fits into same space
         GarbageSize += OldLen - len;
         // Return pointer. Offsets[i] unchanged
         return Buffer + Offsets[i];
      }
      // New string doesn't fit into existing space. Old string becomes garbage
      GarbageSize += OldLen + 1;
   }
   // Put new string at end of Buffer
   if (DataSize + len + 1 > BufferSize) {
      // Make Buffer bigger
      ReserveBuf(DataSize + len + 1);
   }
   // New offset
   Offsets[i] = Top = DataSize;
   DataSize += len + 1;
   // Return pointer
   return Buffer + Top;
}


// Format string using printf style formatting
StringPoolS::StringElement const & StringPoolS::StringElement::Printf(const char * format, ...) {
   // Temporary buffer for new string. 
   // You may change the maximum length FormatLength defined above
   char strbuf[StringPoolS::FormatLength+1];
   // Variable argument list
   va_list args;
   va_start(args, format);
   // Write formatted string to strbuf
   int len = vsnprintf(strbuf, StringPoolS::FormatLength, format, args);
   // Check for errors (len < 0 indicates an error in vsnprintf)
   if ((unsigned int)len > StringPoolS::FormatLength) arr.Error(4, len);
   // Terminate string
   strbuf[StringPoolS::FormatLength] = 0;
   // Put into string pool
   arr.Set(index, strbuf);  
   return *this;
};


// Write string to file or standard output
int StringPoolS::StringElement::Write(FILE * f) {
   return fprintf(f, "%s", (PChar)arr.Get(index));
}


// Search in string for any substring.
// The return value is the position of the substring if found,
// or -1 if not found.
int StringPoolS::StringElement::SearchForSubstring(PChar s) {
   PChar h = arr.Get(index);
   PChar n = strstr_(h, s);
   if (n) {
      // Substring found. Return offset
      return n - h;
   }
   // Not found
   return -1;
}


// Set string to a part of another string
StringPoolS::StringElement const & StringPoolS::StringElement::SetToSubstring(PChar s, int start, int len) {
   // Get length of full string s
   int Len1 = 0; 
   if (s) Len1 = strlen_(s);
   if (start + len > Len1 || start < 0 || len < 0) {
      // Outside bounds of string s
      arr.Error(2, start + len);
   }
   // Save substring
   arr.Set(index, s + start, len);
   return *this;
}


// Operator '[]' lets you read or write a single character in the string.
char & StringPoolS::StringElement::operator[] (int i) {   
   if ((unsigned int)i >= (unsigned int)Len()) {
      // Index out of range
      arr.Error(2, i);
   }
   return arr.Get(index)[i];
}



/******************************************************************************
 3. Example part. Remove this from final application:
******************************************************************************/

// Here follows a working example of how to use StringPoolS. 
// To run this example, just compile this file for console mode and run it.
// You may play with this example as you like

int main() {
   int i, j, n;                        // Indices

   // Declare a string pool
   StringPoolS strings;

   // Each string in the pool has an index and is accessed with strings.[index]
   // A string is stored with the '=' operator:
   strings[4] = "Hello ";               
   // This saves "Hello " as string number 4.

   // Strings are concatenated with '+=':
   strings[4] += "Dolly";               
   // This gives "Hello Dolly".
   // We cannot concatenate strings with '+' because this would require
   // allocation of a separate memory block for temporary storage,
   // which is inefficient. Instead we use '+='.

   // Strings are easily copied
   strings[5] = strings[4];

   // To search in a string:              
   j = strings[5].SearchForSubstring("Doll");
   // Now j is the index to "Doll" in "Hello Dolly".
   // The index starts at 0, so j = 6.
   // j will be -1 if no matching substring is found

   // A single character in a string is accessed with []
   if (j >= 0) strings[5][j] = 'M';     
   // This will change "Hello Dolly" to "Hello Molly"

   // strings[i] is of type StringElement, but it is converted
   // automatically to type 'const char *' whenever this makes sense.
   // We can use this as input to any function that accepts a parameter of
   // type 'const char *' without explicit type conversion. For example,
   // to search for a character in a string:
   if (strchr(strings[4], 'f')) printf("String contains 'f'");

   // The only place where explicit type conversion is needed is in functions
   // like printf that accept parameters of any type:
   printf("\n%s\n\n", (const char *)strings[4]);
   // Without the explicit type conversion, printf would attempt to print an 
   // object of type StringElement which would produce a nonsense output.

   // The number of strings in the string pool is obtained with GetNum():
   n = strings.GetNum();
   // This is the same as the highest used index + 1, in this case n = 6.
   // All unused string indices below n are set to empty strings, so now
   // strings[0] through strings[3] all contain the empty string "".

   // This can be used as an index for a new string, because it is the first
   // unused index:
   strings[n] = "Goodbye";

   // We can extract a substring with SetToSubstring:
   strings[7].SetToSubstring(strings[4], j, 5);
   // This will extract "Molly" from "Hello Molly" and store it in strings[7]

   // To loop through all strings, we start at 0 and end at GetNum()-1:
   for (i = 0; i < strings.GetNum(); i++) {

      // We can put numbers into strings with Printf using a standard 
      // printf style format string (See any C++ manual on printf):
      strings[i].Printf("%2i: %s\n", i, (const char *)strings[i]);
      // Remember to explicitly convert any string to 'const char *' when
      // used as an argument to Printf

      // Print string to standard output
      strings[i].Write(stdout);
   }

   // Remember that any pointer to a string may become invalid when 
   // some other string is modified. For example:
   const char * p = strings[5];
   strings[6] = "Something";
   // printf("%s", p); // Wrong!
   // Here, p may become invalid if the modification of strings[6]
   // causes a re-allocation of memory. A string should always be 
   // identified by its number, not by a pointer.

   return 0;
}
