#include <string.h>
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
   
static void assert (int good, const char *text) {
   if (!good) {
       printhead ("***", text);
       errorcount += 1;
   }
}

static void assertsame (const char *s1, const char *s2, const char *text) {
   if (strcmp (s1, s2)) {
       printhead ("***", 0);
       printf ("%s: %s and %s are different\n", text, s1, s2);
       errorcount += 1;
   }
}

static void starttest (const char *text) {
   printhead ("===", text);
   indent += 1;
}

static void endtest (void) {
   indent -= 1;
}

int main (int argc, const char *argv[]) {

   char buffer[16];
   const char *ref = "Hello world!";
   const char *reflong = "Hello very terribly horribly long world!";

   starttest ("Testing stpecpy()");
   starttest ("Positive use case");
   buffer[0] = 0; //make sure a stupid mistake does not trick the test.
   char *end = buffer + sizeof(buffer);
   char *p = stpecpy (buffer, end, ref);
   assert (p == buffer + strlen(ref), "invalid return pointer");
   assertsame (ref, buffer, "stpecpy()");
   printhead ("===", 0); printf ("Result: %s%s\n", buffer, p?"":" (truncated)");
   endtest ();

   starttest ("Truncate");
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
   endtest ();

   starttest ("Testing strtcpy()");
   starttest ("Testing positive use case");
   buffer[0] = 0; //make sure a stupid mistake does not trick the test.
   int n = strtcpy (buffer, ref, sizeof(buffer));
   assert (n == strlen (ref), "invalid length");
   assertsame (ref, buffer, "strtcpy(), truncated:");
   printhead ("===", 0); printf ("Result: %s%s\n", buffer, (n > 0)?"":" (truncated)");
   endtest ();

   starttest ("Testing truncate");
   buffer[0] = 0; //make sure a stupid mistake does not trick the test.
   n = strtcpy (buffer, reflong, sizeof(buffer));
   assert (n < 0, "not truncated?");
   assert (strlen(buffer) == sizeof(buffer)-1, "not properly truncated");
   printhead ("===", 0); printf ("Result: %s%s\n", buffer, (n > 0)?"":" (truncated)");
   endtest ();

   starttest ("Testing null dst, src or dsize");
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
   char result[32];
   end = result + sizeof(result);
   long long i;
   starttest ("Testing numbers 0 to 999");
   for (i = 0; i < 1000; ++i) {
      snprintf (reference, sizeof(reference), "%lld", i);
      stpedec (result, end, i);
      assertsame (reference, result, "stpedec()");
      if (i == sample) {
          printhead ("===", 0);
          printf ("%lld is printed as %s\n", i, result);
      }
   }
   endtest ();
   starttest ("Testing numbers between 2351 and 1000000");
   sample = 79190;
   for (i = 2351; i < 1000000; i += 5531) {
      snprintf (reference, sizeof(reference), "%lld", i);
      stpedec (result, end, i);
      assertsame (reference, result, "stpedec()");
      if (sample && (i > sample)) {
          printhead ("===", 0);
          printf ("%lld is printed as %s\n", i, result);
          sample = 0;
      }
   }
   endtest ();
   starttest ("Testing numbers between 235123456 and 1000000000000)");
   sample = 791912345678;
   for (i = 235123456; i < 1000000000000; i += 1234565531) {
      snprintf (reference, sizeof(reference), "%lld", i);
      stpedec (result, end, i);
      assertsame (reference, result, "stpedec()");
      if (sample && (i > sample)) {
          printhead ("===", 0);
          printf ("%lld is printed as %s\n", i, result);
          sample = 0;
      }
   }
   endtest ();
   starttest ("Testing negative numbers");
   static long long Samples[] = {-1, -10, -50, -234, -91234567890};
   for (i = 0; i < 5; ++i) {
      snprintf (reference, sizeof(reference), "%lld", Samples[i]);
      stpedec (result, end, Samples[i]);
      assertsame (reference, result, "stpedec()");
      printhead ("===", 0);
      printf ("%lld is printed as %s\n", Samples[i], result);
   }
   endtest ();
   starttest ("Truncate");
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
   assertsame ("1", buffer, "truncation test for 1");
   p = stpedec (buffer, buffer+3, 12);
   assert (p != 0, "12 truncated?");
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
   assertsame ("-12", buffer, "truncation test for -12");
   p = stpedec (buffer, buffer+4, 1);
   assert (p != 0, "1 truncated?");
   assertsame ("1", buffer, "truncation test for 1");
   p = stpedec (buffer, buffer+4, 12);
   assert (p != 0, "12 truncated?");
   assertsame ("12", buffer, "truncation test for 12");
   p = stpedec (buffer, buffer+4, 123);
   assert (p != 0, "123 truncated?");
   assertsame ("123", buffer, "truncation test for 123");
   p = stpedec (buffer, buffer+4, 1234);
   assert (p == 0, "1234 not truncated?");
   assert (strlen(buffer) == 3, "1234 not properly truncated");
   p = stpedec (buffer, buffer+4, 12345);
   assert (p == 0, "12345 not truncated?");
   assert (strlen(buffer) == 3, "12345 not properly truncated");
   endtest ();
   endtest ();
   starttest ("Testing stpedec() performances");
   struct timeval start;
   struct timeval end1;
   struct timeval end2;
   struct timeval end3;
   struct timeval end4;
   struct timeval end5;
   struct timeval end6;
   gettimeofday (&start, 0);
   for (i = 0; i < 1000000; ++i) {
      snprintf (reference, sizeof(reference), "%lld", i);
   }
   gettimeofday (&end1, 0);
   for (i = 0; i < 1000000; ++i) {
      stpedec (result, end, i);
   }
   gettimeofday (&end2, 0);
   for (i = 0; i < 1000000; ++i) {
      snprintf (reference, sizeof(reference), "%lld", 0);
   }
   gettimeofday (&end3, 0);
   for (i = 0; i < 1000000; ++i) {
      stpedec (result, end, 0);
   }
   gettimeofday (&end4, 0);
   for (i = 0; i < 1000000; ++i) {
      snprintf (reference, sizeof(reference), "%lld", 791912345678);
   }
   gettimeofday (&end5, 0);
   for (i = 0; i < 1000000; ++i) {
      stpedec (result, end, 791912345678);
   }
   gettimeofday (&end6, 0);
   long long elapsed = (end1.tv_sec - start.tv_sec) * 1000
                          + (end1.tv_usec - start.tv_usec) / 1000;
   printhead ("===", 0);
   printf ("snprintf() incremented: %lld ms for 1M iterations\n", elapsed);
   elapsed = (end2.tv_sec - end1.tv_sec) * 1000
                + (end2.tv_usec - end1.tv_usec) / 1000;
   printhead ("===", 0);
   printf ("stpedec() incremented: %lld ms for 1M iterations\n", elapsed);
   elapsed = (end3.tv_sec - end2.tv_sec) * 1000
                + (end3.tv_usec - end2.tv_usec) / 1000;
   printhead ("===", 0);
   printf ("snprintf() for value 0: %lld ms for 1M iterations\n", elapsed);
   elapsed = (end4.tv_sec - end3.tv_sec) * 1000
                + (end4.tv_usec - end3.tv_usec) / 1000;
   printhead ("===", 0);
   printf ("stpedec() for value 0: %lld ms for 1M iterations\n", elapsed);
   elapsed = (end5.tv_sec - end4.tv_sec) * 1000
                + (end5.tv_usec - end4.tv_usec) / 1000;
   printhead ("===", 0);
   printf ("snprintf() for value 791912345678: %lld ms for 1M iterations\n", elapsed);
   elapsed = (end6.tv_sec - end5.tv_sec) * 1000
                + (end6.tv_usec - end5.tv_usec) / 1000;
   printhead ("===", 0);
   printf ("stpedec() for value 791912345678: %lld ms for 1M iterations\n", elapsed);
   endtest ();
   endtest ();

   if (errorcount > 0) printf ("*** Test failed after %d errors\n", errorcount);
   else printf ("=== Test passed, no error\n");
   return errorcount;
}

