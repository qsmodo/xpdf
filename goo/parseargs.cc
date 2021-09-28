/*
 * parseargs.h
 *
 * Command line argument parser.
 *
 * Copyright 1996-2003 Glyph & Cog, LLC
 * Copyright 2020 Adam Sampson <ats@offog.org>
 */

#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "parseargs.h"

static ArgDesc *findArg(ArgDesc *args, char *arg);
static bool grabArg(ArgDesc *arg, int i, int *argc, char *argv[]);

bool parseArgs(ArgDesc *args, int *argc, char *argv[]) {
  ArgDesc *arg;
  int i, j;
  bool ok;

  ok = true;
  i = 1;
  while (i < *argc) {
    if (!strcmp(argv[i], "--")) {
      --*argc;
      for (j = i; j < *argc; ++j)
	argv[j] = argv[j+1];
      break;
    } else if ((arg = findArg(args, argv[i]))) {
      if (!grabArg(arg, i, argc, argv))
	ok = false;
    } else {
      ++i;
    }
  }
  return ok;
}

void printUsage(const char *program, const char *otherArgs, ArgDesc *args) {
  ArgDesc *arg;
  char *typ;
  int w, w1;

  w = 0;
  for (arg = args; arg->arg; ++arg) {
    if ((w1 = (int)strlen(arg->arg)) > w)
      w = w1;
  }

  fprintf(stderr, "Usage: %s [options]", program);
  if (otherArgs)
    fprintf(stderr, " %s", otherArgs);
  fprintf(stderr, "\n");

  for (arg = args; arg->arg; ++arg) {
    if (arg->kind == argObsolete || arg->kind == argObsolete1)
      continue;
    fprintf(stderr, "  %s", arg->arg);
    w1 = 9 + w - (int)strlen(arg->arg);
    switch (arg->kind) {
    case argInt:
    case argIntDummy:
      typ = " <int>";
      break;
    case argFP:
    case argFPDummy:
      typ = " <fp>";
      break;
    case argString:
    case argStringDummy:
      typ = " <string>";
      break;
    case argFlag:
    case argFlagDummy:
    default:
      typ = "";
      break;
    }
    fprintf(stderr, "%-*s", w1, typ);
    if (arg->usage)
      fprintf(stderr, ": %s", arg->usage);
    fprintf(stderr, "\n");
  }
}

static ArgDesc *findArg(ArgDesc *args, char *arg) {
  ArgDesc *p;

  for (p = args; p->arg; ++p) {
    if (p->kind < argFlagDummy && !strcmp(p->arg, arg))
      return p;
  }
  return NULL;
}

static bool grabArg(ArgDesc *arg, int i, int *argc, char *argv[]) {
  int n;
  int j;
  bool ok;

  ok = true;
  switch (arg->kind) {
  case argFlag:
    *(bool *)arg->val = true;
    n = 1;
    break;
  case argInt:
    if (i + 1 < *argc && isInt(argv[i+1])) {
      *(int *)arg->val = atoi(argv[i+1]);
      n = 2;
    } else {
      ok = false;
      n = 1;
    }
    break;
  case argFP:
    if (i + 1 < *argc && isFP(argv[i+1])) {
      *(double *)arg->val = atof(argv[i+1]);
      n = 2;
    } else {
      ok = false;
      n = 1;
    }
    break;
  case argString:
    if (i + 1 < *argc) {
      strncpy((char *)arg->val, argv[i+1], arg->size - 1);
      ((char *)arg->val)[arg->size - 1] = '\0';
      n = 2;
    } else {
      ok = false;
      n = 1;
    }
    break;
  case argObsolete:
  case argObsolete1:
    fprintf(stderr, "Argument '%s' is not supported by the Poppler version of xpdf\n", argv[i]);
    ok = false;
    n = 1;
    break;
  default:
    fprintf(stderr, "Internal error in arg table\n");
    n = 1;
    break;
  }
  if (n > 0) {
    *argc -= n;
    for (j = i; j < *argc; ++j)
      argv[j] = argv[j+n];
  }
  return ok;
}

bool isInt(char *s) {
  if (*s == '-' || *s == '+')
    ++s;
  while (isdigit(*s & 0xff))
    ++s;
  if (*s)
    return false;
  return true;
}

bool isFP(char *s) {
  int n;

  if (*s == '-' || *s == '+')
    ++s;
  n = 0;
  while (isdigit(*s & 0xff)) {
    ++s;
    ++n;
  }
  if (*s == '.')
    ++s;
  while (isdigit(*s & 0xff)) {
    ++s;
    ++n;
  }
  if (n > 0 && (*s == 'e' || *s == 'E')) {
    ++s;
    if (*s == '-' || *s == '+')
      ++s;
    if (!isdigit(*s & 0xff))
      return false;
    do {
      ++s;
    } while (isdigit(*s & 0xff));
  }
  if (*s)
    return false;
  return true;
}
