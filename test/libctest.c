#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/time.h>

#include "echttp_libc.h"

static int errorcount = 0;
static int indent = 0;

static void printhead (const char *marker, const char *text) {
   printf ("%s ", marker);
   int i;
   for (i = 0; i < indent; ++i) printf ("   ");
   if (text) printf ("%s\n", text);
}
   
static void printerror (const char *text, const char *value) {
   char formatted[256];
   snprintf (formatted, sizeof(formatted),
             "%s failed for value %s", text, value);
   printhead ("***", formatted);
   errorcount += 1;
}

static void assert (int good, const char *text) {
   const char *preamble = "===";
   if (!good) {
       preamble = "***";
       errorcount += 1;
   }
   printhead (preamble, text);
}

static void assertsame (const char *s1, const char *s2, const char *text) {
   const char *preamble = "===";
   if (strcmp (s1, s2)) {
       preamble = "***";
       printf ("%s and %s are different\n", text, s1, s2);
       errorcount += 1;
   }
   printhead (preamble, text);
}

static void showperformance (struct timeval *start,
                             struct timeval *end,
                             const char *action, int count) {
   long long elapsed = (end->tv_sec - start->tv_sec) * 1000
                          + (end->tv_usec - start->tv_usec) / 1000;
   printhead ("===", 0);
   printf ("%s: %lld ms for %d iterations\n", action, elapsed, count);
}

static void starttest (const char *text) {
   printhead ("===", text);
   indent += 1;
}

static void endtest (void) {
   indent -= 1;
}

// This function is intended to fool the gcc optimizer, which
// has special cases for intrinsic functions..
__attribute__((noinline)) int stringcasecompare (const char *s1, const char *s2) {
   return strcasecmp (s1, s2);
}

// The same treatement is applied to strsame() for fairness.
__attribute__((noinline)) int stringsame (const char *s1, const char *s2) {
   return strsame (s1, s2);
}

int main (int argc, const char *argv[]) {

   char buffer[22];
   char *end = buffer + sizeof(buffer);

   const char *ref = "Hello world!";
   const char *reflong = "Hello very terribly horribly long world!";

   starttest ("Testing stpecpy()");
   starttest ("Positive use case");
   buffer[0] = 0; //make sure a stupid mistake does not trick the test.
   char *p = stpecpy (buffer, end, ref);
   assert (p == buffer + strlen(ref), "invalid return pointer");
   assertsame (ref, buffer, "stpecpy()");
   printhead ("===", 0); printf ("Result: %s%s\n", buffer, p?"":" (truncated)");
   endtest ();

   starttest ("Truncate cases");
   buffer[0] = 0; //make sure a stupid mistake does not trick the test.
   p = stpecpy (buffer, end, reflong);
   assert (p == 0, "not truncated?");
   assert (strlen(buffer) == sizeof(buffer)-1, "not properly truncated");
   printhead ("===", 0); printf ("Result: %s%s\n", buffer, p?"":" (truncated)");
   endtest ();

   starttest ("Concatenation");
   buffer[0] = 0; //make sure a stupid mistake does not trick the test.
   p = stpecpy (buffer, end, "Hello ");
   p = stpecpy (p, end, "world");
   p = stpecpy (p, end, "!");
   assert (p == buffer + strlen(ref), "invalid return pointer");
   assertsame (ref, buffer, "stpecpy(), concatened");
   printhead ("===", 0); printf ("Result: %s%s\n", buffer, p?"":" (truncated)");
   endtest ();

   starttest ("Truncated concatenation");
   buffer[0] = 0; //make sure a stupid mistake does not trick the test.
   p = stpecpy (buffer, end, "Hello ");
   p = stpecpy (p, end, "very terribly ");
   p = stpecpy (p, end, "horribly long ");
   p = stpecpy (p, end, "world!");
   assert (p == 0, "not truncated?");
   assert (strlen(buffer) == sizeof(buffer)-1, "not properly truncated");
   printhead ("===", 0); printf ("Result: %s%s\n", buffer, p?"":" (truncated)");
   endtest ();

   starttest ("Null dst, end or src");
   buffer[0] = 0; //make sure a stupid mistake does not trick the test.
   p = stpecpy (0, end, ref);
   assert (p == 0, "copied something to address 0?");
   p = stpecpy (buffer, 0, ref);
   assert (p == 0, "copied something to an empty buffer?");
   p = stpecpy (buffer, end, 0);
   assert (p == buffer, "invalid return pointer");
   assert (strlen(buffer) == 0, "copied something from address 0?");
   endtest ();
   endtest ();

   starttest ("Testing strtcpy()");
   starttest ("Positive use case");
   buffer[0] = 0; //make sure a stupid mistake does not trick the test.
   int n = strtcpy (buffer, ref, sizeof(buffer));
   assert (n == strlen (ref), "invalid length");
   assertsame (ref, buffer, "strtcpy(), truncated:");
   printhead ("===", 0); printf ("Result: %s%s\n", buffer, (n > 0)?"":" (truncated)");
   endtest ();

   starttest ("Truncate case");
   buffer[0] = 0; //make sure a stupid mistake does not trick the test.
   n = strtcpy (buffer, reflong, sizeof(buffer));
   assert (n < 0, "not truncated?");
   assert (strlen(buffer) == sizeof(buffer)-1, "not properly truncated");
   printhead ("===", 0); printf ("Result: %s%s\n", buffer, (n > 0)?"":" (truncated)");
   endtest ();

   starttest ("Null dst, src or dsize");
   buffer[0] = 0; //make sure a stupid mistake does not trick the test.
   n = strtcpy (0, ref, sizeof(buffer));
   assert (n < 0, "copied something to address 0?");
   n = strtcpy (buffer, 0, sizeof(buffer));
   assert (n < 0, "copied something from address 0?");
   n = strtcpy (buffer, ref, 0);
   assert (n < 0, "copied something to buffer of length 0?");
   endtest ();
   endtest ();

   starttest ("Testing stpedec()");
   long long sample = 765;
   char reference[32];
   long long i;
   starttest ("Numbers 0 to 999");
   for (i = 0; i < 1000; ++i) {
      snprintf (reference, sizeof(reference), "%lld", i);
      p = stpedec (buffer, end, i);
      assert (p == buffer + strlen(reference), "invalid return pointer");
      assertsame (reference, buffer, "stpedec()");
      if (i == sample) {
          printhead ("===", 0);
          printf ("%lld is printed as %s\n", i, buffer);
      }
   }
   endtest ();
   starttest ("Numbers between 1000 and 1000000");
   sample = 79190;
   for (i = 1000; i < 1000000; i += 5531) {
      snprintf (reference, sizeof(reference), "%lld", i);
      p = stpedec (buffer, end, i);
      assert (p == buffer + strlen(reference), "invalid return pointer");
      assertsame (reference, buffer, "stpedec()");
      if (sample && (i > sample)) {
          printhead ("===", 0);
          printf ("%lld is printed as %s\n", i, buffer);
          sample = 0;
      }
   }
   endtest ();
   starttest ("Numbers between 235123456 and 1000000000000");
   sample = 791912345678;
   for (i = 235123456; i < 1000000000000; i += 1234565531) {
      snprintf (reference, sizeof(reference), "%lld", i);
      p = stpedec (buffer, end, i);
      assert (p == buffer + strlen(reference), "invalid return pointer");
      assertsame (reference, buffer, "stpedec()");
      if (sample && (i > sample)) {
          printhead ("===", 0);
          printf ("%lld is printed as %s\n", i, buffer);
          sample = 0;
      }
   }
   endtest ();
   starttest ("Negative numbers");
   static long long Samples[] = {-1, -10, -50, -234, -91234567890};
   for (i = 0; i < 5; ++i) {
      snprintf (reference, sizeof(reference), "%lld", Samples[i]);
      p = stpedec (buffer, end, Samples[i]);
      assert (p == buffer + strlen(reference), "invalid return pointer");
      assertsame (reference, buffer, "stpedec()");
      printhead ("===", 0);
      printf ("%lld is printed as %s\n", Samples[i], buffer);
   }
   starttest ("INT64_MIN, INT64_MAX");
   snprintf (reference, sizeof(reference), "%lld", INT64_MIN);
   p = stpedec (buffer, end, INT64_MIN);
   assert (p == buffer + strlen(reference), "invalid return pointer");
   assertsame (reference, buffer, "stpedec()");
   printhead ("===", 0);
   printf ("%lld is printed as %s\n", INT64_MIN, buffer);
   snprintf (reference, sizeof(reference), "%lld", INT64_MAX);
   p = stpedec (buffer, end, INT64_MAX);
   assert (p == buffer + strlen(reference), "invalid return pointer");
   assertsame (reference, buffer, "stpedec()");
   printhead ("===", 0);
   printf ("%lld is printed as %s\n", INT64_MAX, buffer);
   endtest ();
   endtest ();
   starttest ("Truncate cases");
   starttest ("Buffer length 1");
   p = stpedec (buffer, buffer+1, -12);
   assert (p == 0, "-12 not truncated?");
   assert (strlen(buffer) == 0, "-12 not properly truncated");
   p = stpedec (buffer, buffer+1, -1);
   assert (p == 0, "-1 not truncated?");
   assert (strlen(buffer) == 0, "-1 not properly truncated");
   p = stpedec (buffer, buffer+1, 1);
   assert (p == 0, "1 not truncated?");
   assert (strlen(buffer) == 0, "1 not properly truncated");
   p = stpedec (buffer, buffer+1, 12);
   assert (p == 0, "12 not truncated?");
   assert (strlen(buffer) == 0, "12 not properly truncated");
   p = stpedec (buffer, buffer+1, 123);
   assert (p == 0, "123 not truncated?");
   assert (strlen(buffer) == 0, "123 not properly truncated");
   p = stpedec (buffer, buffer+1, 1234);
   assert (p == 0, "1234 not truncated?");
   assert (strlen(buffer) == 0, "1234 not properly truncated");
   endtest ();
   starttest ("Buffer length 2");
   p = stpedec (buffer, buffer+2, -123);
   assert (p == 0, "-123 not truncated?");
   assert (strlen(buffer) == 1, "-123 not properly truncated");
   p = stpedec (buffer, buffer+2, -12);
   assert (p == 0, "-12 not truncated?");
   assert (strlen(buffer) == 1, "-12 not properly truncated");
   p = stpedec (buffer, buffer+2, -1);
   assert (p == 0, "-1 not truncated?");
   assert (strlen(buffer) == 1, "-1 not properly truncated");
   p = stpedec (buffer, buffer+2, 1);
   assert (p != 0, "1 truncated?");
   assert (p == buffer + strlen(buffer), "invalid return pointer");
   assertsame ("1", buffer, "truncation test for 1");
   p = stpedec (buffer, buffer+2, 12);
   assert (p == 0, "1234 not truncated?");
   assert (strlen(buffer) == 1, "12 not properly truncated");
   p = stpedec (buffer, buffer+2, 123);
   assert (p == 0, "123 not truncated?");
   assert (strlen(buffer) == 1, "123 not properly truncated");
   p = stpedec (buffer, buffer+2, 1234);
   assert (p == 0, "1234 not truncated?");
   assert (strlen(buffer) == 1, "1234 not properly truncated");
   endtest ();
   starttest ("Buffer length 3");
   p = stpedec (buffer, buffer+3, -123);
   assert (p == 0, "-123 not truncated?");
   assert (strlen(buffer) == 2, "-123 not properly truncated");
   p = stpedec (buffer, buffer+3, -12);
   assert (p == 0, "-12 not truncated?");
   assert (strlen(buffer) == 2, "-12 not properly truncated");
   p = stpedec (buffer, buffer+3, 1);
   assert (p != 0, "1 truncated?");
   assert (p == buffer + strlen(buffer), "invalid return pointer");
   assertsame ("1", buffer, "truncation test for 1");
   p = stpedec (buffer, buffer+3, 12);
   assert (p != 0, "12 truncated?");
   assert (p == buffer + strlen(buffer), "invalid return pointer");
   assertsame ("12", buffer, "truncation test for 12");
   p = stpedec (buffer, buffer+3, 123);
   assert (p == 0, "123 not truncated?");
   assert (strlen(buffer) == 2, "123 not properly truncated");
   p = stpedec (buffer, buffer+3, 1234);
   assert (p == 0, "1234 not truncated?");
   assert (strlen(buffer) == 2, "1234 not properly truncated");
   endtest ();
   starttest ("Buffer length 4");
   p = stpedec (buffer, buffer+4, -123);
   assert (p == 0, "-123 not truncated?");
   assert (strlen(buffer) == 3, "-123 not properly truncated");
   p = stpedec (buffer, buffer+4, -12);
   assert (p != 0, "-12 truncated?");
   assert (p == buffer + strlen(buffer), "invalid return pointer");
   assertsame ("-12", buffer, "truncation test for -12");
   p = stpedec (buffer, buffer+4, 1);
   assert (p != 0, "1 truncated?");
   assert (p == buffer + strlen(buffer), "invalid return pointer");
   assertsame ("1", buffer, "truncation test for 1");
   p = stpedec (buffer, buffer+4, 12);
   assert (p != 0, "12 truncated?");
   assert (p == buffer + strlen(buffer), "invalid return pointer");
   assertsame ("12", buffer, "truncation test for 12");
   p = stpedec (buffer, buffer+4, 123);
   assert (p != 0, "123 truncated?");
   assert (p == buffer + strlen(buffer), "invalid return pointer");
   assertsame ("123", buffer, "truncation test for 123");
   p = stpedec (buffer, buffer+4, 1234);
   assert (p == 0, "1234 not truncated?");
   assert (strlen(buffer) == 3, "1234 not properly truncated");
   p = stpedec (buffer, buffer+4, 12345);
   assert (p == 0, "12345 not truncated?");
   assert (strlen(buffer) == 3, "12345 not properly truncated");
   endtest ();
   endtest ();
   starttest ("Null dst, end or src");
   buffer[0] = 0; //make sure a stupid mistake does not trick the test.
   p = stpedec (0, end, 1);
   assert (p == 0, "copied something to address 0?");
   p = stpedec (buffer, 0, 1);
   assert (p == 0, "copied something to an empty buffer?");
   endtest ();
   starttest ("Performances");
   struct timeval start;
   struct timeval end1;
   struct timeval end2;
   struct timeval end3;
   struct timeval end4;
   struct timeval end5;
   struct timeval end6;
   struct timeval end7;
   gettimeofday (&start, 0);
   for (i = 0; i < 1000000; ++i) {
      snprintf (reference, sizeof(reference), "%lld", i);
   }
   gettimeofday (&end1, 0);
   for (i = 0; i < 1000000; ++i) {
      stpedec (buffer, end, i);
   }
   gettimeofday (&end2, 0);
   for (i = 0; i < 1000000; ++i) {
      snprintf (reference, sizeof(reference), "%lld", 0);
   }
   gettimeofday (&end3, 0);
   for (i = 0; i < 1000000; ++i) {
      stpedec (buffer, end, 0);
   }
   gettimeofday (&end4, 0);
   for (i = 0; i < 1000000; ++i) {
      snprintf (reference, sizeof(reference), "%lld", 791912345678);
   }
   gettimeofday (&end5, 0);
   for (i = 0; i < 1000000; ++i) {
      stpedec (buffer, end, 791912345678);
   }
   gettimeofday (&end6, 0);
   for (i = 0; i < 1000000; ++i) {
      stpedec (buffer, end, 66);
   }
   gettimeofday (&end7, 0);
   showperformance (&start, &end1, "snprintf() incremented", 1000000);
   showperformance (&end1, &end2, "stpedec() incremented", 1000000);
   showperformance (&end2, &end3, "snprintf() with value 0", 1000000);
   showperformance (&end3, &end4, "stpdec() with value 0", 1000000);
   showperformance (&end4, &end5, "snprintf() with value 791912345678", 1000000);
   showperformance (&end5, &end6, "stpdec() with value 791912345678", 1000000);
   showperformance (&end6, &end7, "stpdec() with value 66", 1000000);
   endtest ();
   endtest ();

   starttest ("Testing strsame()");
   starttest ("Testing strsame() with null pointers");
   assert (strsame(0,0) == 0, "strsame(null,null)");
   assert (strsame("whatever",0) == 0, "strsame(string,null)");
   assert (strsame(0,"whatever") == 0, "strsame(null,string)");
   endtest ();
   starttest ("Testing strsame() with equal pointers");
   const char *p1 = "whatSoever";
   assert (strsame(p1,p1), "strsame(p1,p1)");
   endtest ();
   starttest ("Testing strsame() with same content");
   char p2[60];
   snprintf (p2, sizeof(p2), p1);
   assert (strsame(p1,p2), "strsame(whatSoever,whatSoever)");
   assert (strsame(p1,"whatsoever"), "strsame(whatSoever,whatsoever)");
   endtest ();
   starttest ("Testing strsame() with different strings");
   assert (strsame(p1,"whats0ever") == 0, "strsame(whatSoever,whats0ever)");
   assert (strsame(p1,"somethingelse") == 0, "strsame(whatSoever,somethingelse)");
   assert (strsame(p1,"short") == 0, "strsame(whatSoever,short)");
   endtest ();

   starttest ("Performances");
   for (i = 0; i < 10; ++i) { // Prime the cache.
      if (!strsame (p1, p2)) printf ("*** mismatch at %d!\n", i);
      if (stringcasecompare (p1, p2)) printf ("*** mismatch at %d!\n", i);
   }
   gettimeofday (&start, 0);
   for (i = 0; i < 1000000; ++i) {
      if (!stringsame (p1, p1)) printf ("*** mismatch at %d!\n", i);
   }
   gettimeofday (&end1, 0);
   for (i = 0; i < 1000000; ++i) {
      if (stringcasecompare (p1, p1)) printf ("*** mismatch at %d!\n", i);
   }
   gettimeofday (&end2, 0);
   for (i = 0; i < 1000000; ++i) {
      if (!stringsame (p1, p2)) printf ("*** mismatch at %d!\n", i);
   }
   gettimeofday (&end3, 0);
   for (i = 0; i < 1000000; ++i) {
      if (stringcasecompare (p1, p2)) printf ("*** mismatch at %d!\n", i);
   }
   gettimeofday (&end4, 0);
   showperformance (&start, &end1, "strsame() with same pointer", 1000000);
   showperformance (&end1, &end2, "strcasecmp() with same pointer", 1000000);
   showperformance (&end2, &end3, "strsame() with same content", 1000000);
   showperformance (&end3, &end4, "strcasecmp() with same content", 1000000);
   endtest ();
   endtest ();

   starttest ("Testing hexadecimal to raw byte conversion");
   static const char *hexref[] = {
       "00", "01", "02", "03", "04", "05", "06", "07",
       "08", "09", "0a", "0B", "0c", "0D", "0e", "0F",
       "10", "11", "12", "13", "14", "15", "16", "17",
       "18", "19", "1a", "1B", "1c", "1D", "1e", "1F",
       "20", "21", "22", "23", "24", "25", "26", "27",
       "28", "29", "2a", "2B", "2c", "2D", "2e", "2F",
       "30", "31", "32", "33", "34", "35", "36", "37",
       "38", "39", "3a", "3B", "3c", "3D", "3e", "3F",
       "40", "41", "42", "43", "44", "45", "46", "47",
       "48", "49", "4a", "4B", "4c", "4D", "4e", "4F",
       "50", "51", "52", "53", "54", "55", "56", "57",
       "58", "59", "5a", "5B", "5c", "5D", "5e", "5F",
       "60", "61", "62", "63", "64", "65", "66", "67",
       "68", "69", "6a", "6B", "6c", "6D", "6e", "6F",
       "70", "71", "72", "73", "74", "75", "76", "77",
       "78", "79", "7a", "7B", "7c", "7D", "7e", "7F",
       "80", "81", "82", "83", "84", "85", "86", "87",
       "88", "89", "8a", "8B", "8c", "8D", "8e", "8F",
       "90", "91", "92", "93", "94", "95", "96", "97",
       "98", "99", "9a", "9B", "9c", "9D", "9e", "9F",
       "A0", "A1", "A2", "A3", "A4", "A5", "A6", "A7",
       "a8", "a9", "aa", "aB", "ac", "aD", "ae", "aF",
       "B0", "B1", "B2", "B3", "B4", "B5", "B6", "B7",
       "b8", "b9", "ba", "bB", "bc", "bD", "be", "bF",
       "C0", "C1", "C2", "C3", "C4", "C5", "C6", "C7",
       "c8", "c9", "ca", "cB", "cc", "cD", "ce", "cF",
       "D0", "D1", "D2", "D3", "D4", "D5", "D6", "D7",
       "d8", "d9", "da", "dB", "dc", "dD", "de", "dF",
       "E0", "E1", "E2", "E3", "E4", "E5", "E6", "E7",
       "e8", "e9", "ea", "eB", "ec", "eD", "ee", "eF",
       "F0", "F1", "F2", "F3", "F4", "F5", "F6", "F7",
       "f8", "f9", "fa", "fB", "fc", "fD", "fe", "fF"};
   for (i = 0; i < 255; ++i) {
      if (i != hextobyte (hexref[i]))
          printerror ("Convert hex to byte", hexref[i]);
   }
   static const char *hexinvalid[] =
       {"/0", "0:", "0@", "G0", "`0", "g0", "0", "", 0};
   for (i = 0; hexinvalid[i] != 0; ++i) {
       if (hextobyte (hexinvalid[i]) != -1)
           printerror ("Invalid conversion", hexinvalid[i]);
   }
   endtest ();

   if (errorcount > 0) printf ("*** Test failed after %d errors\n", errorcount);
   else printf ("=== Test passed, no error\n");
   return errorcount;
}

