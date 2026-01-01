/*
 * decisionTableCompiler - generate optimal pseudocode for decision tables
 * Copyright (C) 1993-2025 G. David Butler <gdb@dbSystems.com>
 *
 * This file is part of decisionTableCompiler
 *
 * decisionTableCompiler is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * decisionTableCompiler is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include "csv.h"

/********************************************************************************/

typedef struct sym sym_t;

struct sym {
  unsigned int n;
  unsigned char v[1]; /* extended when allocated to n */
};

static const sym_t *
symNew(
  const unsigned char *v
 ,unsigned int l
){
  sym_t *r;

  r = 0;
  if (!l || !v
   || !(r = malloc(l > sizeof (sym_t) - (unsigned long)&((sym_t *)0)->v
                 ? l + (unsigned long)&((sym_t *)0)->v
                 : sizeof (sym_t)
                  )
       )
  ) {
    free(r);
    return (0);
  }
  r->n = l;
  while (l--)
    r->v[l] = *(v + l);
  return (r);
}

#if DTC_DEBUG
static void
symPrt(
  const sym_t *sym
){
  if (!sym)
    return;
  printf("sym:%.*s\n", sym->n, &sym->v[0]);
}
#endif

/* only called by csvCb and symsFre */
static void
symFre(
  const sym_t *sym
){
  if (!sym)
    return;
  free((void *)sym);
}

static int
symCmp(
  const sym_t *e1
 ,const sym_t *e2
){
  unsigned int i;
  unsigned int j;
  int r;

  j = e1->n < e2->n ? e1->n : e2->n;
  for (i = 0; i < j && !(r = e1->v[i] - e2->v[i]); ++i);
  if (r < 0)
    return (-1);
  if (r > 0)
    return (1);
  r = e1->n - e2->n;
  if (r < 0)
    return (-1);
  if (r > 0)
    return (1);
  return (0);
}

/********************************************************************************/

typedef struct syms syms_t;

struct syms {
  const sym_t **v;
  unsigned int n;
};

static syms_t *
symsNew(
  void
){
  return (calloc(1, sizeof (syms_t)));
}

static int
symsSchCmp(
  const sym_t *k
 ,const sym_t **e
){
  return (symCmp(k, *e));
}

static int
symsSrtCmp(
  const sym_t **e1
 ,const sym_t **e2
){
  return (symCmp(*e1, *e2));
}

static const sym_t *
symsAdd(
  syms_t *syms
 ,const sym_t *sym
){
  const sym_t **r;
  void *v;

  if (!sym || !syms)
    return (0);
  if ((r = bsearch(sym, syms->v, syms->n, sizeof (*syms->v), (int(*)(const void *, const void *))symsSchCmp)))
    return (*r);
  if (!(v = realloc(syms->v, (syms->n + 1) * sizeof (*syms->v))))
    return (0);
  syms->v = v;
  *(syms->v + syms->n++) = sym;
  qsort(syms->v, syms->n, sizeof (*syms->v), (int(*)(const void *, const void *))symsSrtCmp);
  return (sym);
}

#if DTC_DEBUG
static void
symsPrt(
  const syms_t *syms
){
  unsigned int i;

  if (!syms)
    return;
  puts("syms(");
  for (i = 0; i < syms->n; ++i)
    symPrt(*(syms->v + i));
  puts(")");
}
#endif

/* only called by csvFre */
static void
symsFre(
  syms_t *syms
){
  if (!syms)
    return;
  while (syms->n--)
    symFre(*(syms->v + syms->n));
  free(syms->v);
  free(syms);
}

/********************************************************************************/

typedef struct nam nam_t;

#if DTC_DEBUG
static void namPrtSym(
  const nam_t *nam
);
#endif

static int namCmp(const nam_t *, const nam_t *);

typedef struct infs infs_t;

static infs_t *infsNew(void);

static void infsRefFre(infs_t *);

/********************************************************************************/

typedef struct val val_t;

struct val {
  const nam_t *nam;
  const sym_t *sym;
  infs_t *infs;
};

static const val_t *
valNew(
  const nam_t *nam
 ,const sym_t *sym
){
  val_t *r;

  if (!nam || !sym
   || !(r = calloc(1, sizeof (*r))))
    return (0);
  r->nam = nam;
  r->sym = sym;
  return (r);
}

#if DTC_DEBUG
static void
valPrt(
  const val_t *val
){
  if (!val)
    return;
  puts("val(");
  namPrtSym(val->nam);
  symPrt(val->sym);
  puts(")");
}
#endif

/* only called by csvCb and valsFre */
static void
valFre(
  const val_t *val
){
  if (!val)
    return;
  infsRefFre(val->infs);
  free((void *)val);
}

static int
valCmp(
  const val_t *e1
 ,const val_t *e2
){
  int r;

  if ((r = namCmp(e1->nam, e2->nam)))
    return (r);
  return (symCmp(e1->sym, e2->sym));
}

/********************************************************************************/

typedef struct vals vals_t;

struct vals {
  const val_t **v;
  unsigned int n;
};

static vals_t *
valsNew(
  void
){
  return (calloc(1, sizeof (vals_t)));
}

static int
valsSchCmp(
  const val_t *k
 ,const val_t **e
){
  return (valCmp(k, *e));
}

static int
valsSrtCmp(
  const val_t **e1
 ,const val_t **e2
){
  return (valCmp(*e1, *e2));
}

static const val_t *
valsAdd(
  vals_t *vals
 ,const val_t *val
){
  void *v;
  unsigned int lo;
  unsigned int hi;
  unsigned int mid;
  int c;

  if (!val || !vals)
    return (0);
  /* binary search for insertion position - O(n) vs qsort O(n log n) */
  lo = 0;
  hi = vals->n;
  while (lo < hi) {
    mid = lo + (hi - lo) / 2;
    c = valsSrtCmp(&val, vals->v + mid);
    if (c == 0)
      return (*(vals->v + mid));
    if (c < 0)
      hi = mid;
    else
      lo = mid + 1;
  }
  if (!(v = realloc(vals->v, (vals->n + 1) * sizeof (*vals->v))))
    return (0);
  vals->v = v;
  if (lo < vals->n)
    memmove(vals->v + lo + 1, vals->v + lo, (vals->n - lo) * sizeof (*vals->v));
  *(vals->v + lo) = val;
  ++vals->n;
  return (val);
#if 0 /* original bsearch/qsort technique - O(n log n) per insert */
  val_t **r;

  if (!val || !vals)
    return (0);
  if ((r = bsearch(val, vals->v, vals->n, sizeof (*vals->v), (int(*)(const void *, const void *))valsSchCmp)))
    return (*r);
  if (!(v = realloc(vals->v, (vals->n + 1) * sizeof (*vals->v))))
    return (0);
  vals->v = v;
  *(vals->v + vals->n++) = val;
  qsort(vals->v, vals->n, sizeof (*vals->v), (int(*)(const void *, const void *))valsSrtCmp);
  return (val);
#endif
}

#if DTC_DEBUG
static void
valsPrt(
  const vals_t *vals
){
  unsigned int i;

  if (!vals)
    return;
  puts("vals(");
  for (i = 0; i < vals->n; ++i)
    valPrt(*(vals->v + i));
  puts(")");
}
#endif

/* only called by namFre */
static void
valsFre(
  vals_t *vals
){
  if (!vals)
    return;
  while (vals->n--)
    valFre(*(vals->v + vals->n));
  free(vals->v);
  free(vals);
}

static vals_t *
valsRefDup(
  const vals_t *vals
){
  vals_t *r;

  if (!(r = valsNew())
   || !(r->v = malloc(vals->n * sizeof (*r->v)))) {
    free(r);
    return (0);
  }
  for (r->n = 0; r->n < vals->n; ++r->n)
    *(r->v + r->n) = *(vals->v + r->n);
  return (r);
}

static void
valsRefFre(
  vals_t *vals
){
  if (!vals)
    return;
  free(vals->v);
  free(vals);
}

static int
valsCmp(
  const vals_t *e1
 ,const vals_t *e2
){
  unsigned int i;
  int r;

  for (i = 0; i < e1->n && i < e2->n; ++i)
    if ((r = valCmp(*(e1->v + i), *(e2->v + i))))
      return (r);
  if (e1->n < e2->n)
    return (-1);
  if (e1->n > e2->n)
    return (1);
  return (0);
}

/********************************************************************************/

struct nam {
  const sym_t *sym;
  vals_t *vals;
};

static const nam_t *
namNew(
  const sym_t *sym
){
  nam_t *r;

  r = 0;
  if (!sym
   || !(r = calloc(1, sizeof (*r)))
   || !(r->vals = valsNew())) {
    free(r);
    return (0);
  }
  r->sym = sym;
  return (r);
}

#if DTC_DEBUG
static void
namPrtSym(
  const nam_t *nam
){
  if (!nam)
    return;
  symPrt(nam->sym);
}
#endif

#if DTC_DEBUG
static void
namPrt(
  const nam_t *nam
){
  if (!nam)
    return;
  puts("nam(");
  symPrt(nam->sym);
  valsPrt(nam->vals);
  puts(")");
}
#endif

/* only called by csvCb and namsFre */
static void
namFre(
  const nam_t *nam
){
  if (!nam)
    return;
  valsFre(nam->vals);
  free((void *)nam);
}

static int
namCmp(
  const nam_t *e1
 ,const nam_t *e2
){
  return (symCmp(e1->sym, e2->sym));
}

/********************************************************************************/

typedef struct nams nams_t;

struct nams {
  const nam_t **v;
  unsigned int n;
};

static nams_t *
namsNew(
  void
){
  return (calloc(1, sizeof (nams_t)));
}

static int
namsSchCmp(
  const nam_t *k
 ,const nam_t **e
){
  return (namCmp(k, *e));
}

static int
namsSrtCmp(
  const nam_t **e1
 ,const nam_t **e2
){
  return (namCmp(*e1, *e2));
}

static const nam_t *
namsAdd(
  nams_t *nams
 ,const nam_t *nam
){
  nam_t **r;
  void *v;

  if (!nam || !nams)
    return (0);
  if ((r = bsearch(nam, nams->v, nams->n, sizeof (*nams->v), (int(*)(const void *, const void *))namsSchCmp)))
    return (*r);
  if (!(v = realloc(nams->v, (nams->n + 1) * sizeof (*nams->v))))
    return (0);
  nams->v = v;
  *(nams->v + nams->n++) = nam;
  qsort(nams->v, nams->n, sizeof (*nams->v), (int(*)(const void *, const void *))namsSrtCmp);
  return (nam);
}

#if DTC_DEBUG
static void
namsPrt(
  const nams_t *nams
){
  unsigned int i;

  if (!nams)
    return;
  puts("nams(");
  for (i = 0; i < nams->n; ++i)
    namPrt(*(nams->v + i));
  puts(")");
}
#endif

/* only called by csvFre */
static void
namsFre(
  nams_t *nams
){
  if (!nams)
    return;
  while (nams->n--)
    namFre(*(nams->v + nams->n));
  free(nams->v);
  free(nams);
}

/********************************************************************************/

typedef struct inf inf_t;

struct inf {
  const val_t *val;
  vals_t *vals;
  const unsigned char *fil;
  unsigned int row;
};

static const inf_t *
infNew(
  const val_t *val
 ,const unsigned char *fil
 ,unsigned int row
){
  inf_t *r;

  if (!val
   || !(r = calloc(1, sizeof (inf_t))))
    return (0);
  if (!(r->vals = valsNew())) {
    free(r);
    return (0);
  }
  r->val = val;
  r->fil = fil;
  r->row = row;
  return (r);
}

#if DTC_DEBUG
static void
infPrt(
  const inf_t *inf
){
  if (!inf)
    return;
  puts("inf(");
  valPrt(inf->val);
  valsPrt(inf->vals);
  puts(")");
}
#endif

/* only called by csvFre and infsFre */
static void
infFre(
  const inf_t *inf
){
  if (!inf)
    return;
  valsRefFre(inf->vals);
  free((void *)inf);
}

static int
infCmp(
  const inf_t *e1
 ,const inf_t *e2
){
  int r;

  if ((r = valCmp(e1->val, e2->val)))
    return (r);
  return (valsCmp(e1->vals, e2->vals));
}

/********************************************************************************/

struct infs {
  const inf_t **v;
  unsigned int n;
};

static infs_t *
infsNew(
  void
){
  return (calloc(1, sizeof (infs_t)));
}

#if 0 /* only used by original bsearch/qsort technique */
static int
infsSchCmp(
  const inf_t *k
 ,const inf_t **e
){
  return (infCmp(k, *e));
}
#endif

static int
infsValSchCmp(
  const val_t *k
 ,const inf_t **e
){
  return (valCmp(k, (*e)->val));
}

static int
infsSrtCmp(
  const inf_t **e1
 ,const inf_t **e2
){
  return (infCmp(*e1, *e2));
}

static const inf_t *
infsAdd(
  infs_t *infs
 ,const inf_t *inf
){
  void *v;
  unsigned int lo;
  unsigned int hi;
  unsigned int mid;
  int c;

  if (!inf || !infs)
    return (0);
  /* binary search for insertion position - O(n) vs qsort O(n log n) */
  lo = 0;
  hi = infs->n;
  while (lo < hi) {
    mid = lo + (hi - lo) / 2;
    c = infsSrtCmp(&inf, infs->v + mid);
    if (c == 0)
      return (*(infs->v + mid));
    if (c < 0)
      hi = mid;
    else
      lo = mid + 1;
  }
  if (!(v = realloc(infs->v, (infs->n + 1) * sizeof (*infs->v))))
    return (0);
  infs->v = v;
  if (lo < infs->n)
    memmove(infs->v + lo + 1, infs->v + lo, (infs->n - lo) * sizeof (*infs->v));
  *(infs->v + lo) = inf;
  ++infs->n;
  return (inf);
#if 0 /* original bsearch/qsort technique - O(n log n) per insert */
  inf_t **r;

  if (!inf || !infs)
    return (0);
  if ((r = bsearch(inf, infs->v, infs->n, sizeof (*infs->v), (int(*)(const void *, const void *))infsSchCmp)))
    return (*r);
  if (!(v = realloc(infs->v, (infs->n + 1) * sizeof (*infs->v))))
    return (0);
  infs->v = v;
  *(infs->v + infs->n++) = inf;
  qsort(infs->v, infs->n, sizeof (*infs->v), (int(*)(const void *, const void *))infsSrtCmp);
  return (inf);
#endif
}

#if DTC_DEBUG
static void
infsPrt(
  const infs_t *infs
){
  unsigned int i;

  if (!infs)
    return;
  puts("infs(");
  for (i = 0; i < infs->n; ++i)
    infPrt(*(infs->v + i));
  puts(")");
}
#endif

/* only called by csvFre */
static void
infsFre(
  infs_t *infs
){
  if (!infs)
    return;
  while (infs->n--)
    infFre(*(infs->v + infs->n));
  free(infs->v);
  free(infs);
}

static infs_t *
infsRefDup(
  const infs_t *infs
){
  infs_t *r;

  if (!(r = infsNew())
   || !(r->v = malloc(infs->n * sizeof (*r->v)))) {
    free(r);
    return (0);
  }
  for (r->n = 0; r->n < infs->n; ++r->n)
    *(r->v + r->n) = *(infs->v + r->n);
  return (r);
}

static void
infsRefFre(
  infs_t *infs
){
  if (!infs)
    return;
  free(infs->v);
  free(infs);
}

static int
infsCmp(
  const infs_t *e1
 ,const infs_t *e2
){
  unsigned int i;
  int r;

  for (i = 0; i < e1->n && i < e2->n; ++i)
    if ((r = infCmp(*(e1->v + i), *(e2->v + i))))
      return (r);
  if (e1->n < e2->n)
    return (-1);
  if (e1->n > e2->n)
    return (1);
  return (0);
}

/********************************************************************************/

struct csv {
  const unsigned char *fil;
  syms_t *syms;
  nams_t *nams;
  infs_t *infs;
  const inf_t *inf;
  const nam_t **col;
  unsigned int coln;
  int inCom;
  int inNam;
};

static int
csvCb(
  csvTp_t t
 ,unsigned int r
 ,unsigned int c
 ,const csvSt_t *l
 ,void *v
){
#define V ((struct csv *)v)
  const sym_t *sym;
  const nam_t *nam;
  const val_t *val;
  unsigned char *d;
  const void *cv;
  void *tv;
  unsigned int j;
  int i;

  (void)v;
  ++r; /* convert row# from zero based to one based */
  switch (t) {
  case csvTp_Ce:
    if (V->inf) {
      const inf_t *inf;

      if (!(inf = infsAdd(V->infs, V->inf))) {
        fprintf(stderr, "infsAdd fail @%s:%u:%u(%.*s)\n", V->fil, r, c, l->l, l->s);
        return (1);
      }
      if (inf != V->inf) {
        fprintf(stderr, "duplicate inf @%s:%u @%s:%u\n", inf->fil, inf->row, V->inf->fil, V->inf->row);
        return (1);
      }
      V->inf = 0;
    }
    break;
  case csvTp_Cb:
    V->inCom = 0;
    V->inNam = 0;
    break;
  case csvTp_Cv:
    if (V->inCom)
      break;
    if (!l->l) {
      if (V->inNam) {
        fprintf(stderr, "Empty name in '@' row @%s:%u:%u\n", V->fil, r, c);
        return (1);
      }
      if (!c) {
        fprintf(stderr, "Empty value @%s:%u:%u\n", V->fil, r, c);
        return (1);
      }
      break; /* don't care */
    }
    if (!c) { /* first column */
      if (*l->s == '#') {
        V->inCom = 1;
        break;
      }
      if (*l->s == '@') {
        if (l->l < 2) {
          fprintf(stderr, "Empty @name @%s:%u:%u\n", V->fil, r, c);
          return (1);
        }
        V->coln = 0;
        V->inNam = 1;
      }
    }
    if (!(d = malloc(l->l))
     || (i = csvDecodeValue(d, l->l, l->s, l->l)) <= 0
     || i > (int)l->l) {
      free(d);
      fprintf(stderr, "csvDecodeValue @%s:%u:%u(%.*s)\n", V->fil, r, c, l->l, l->s);
      return (1);
    }
    if (V->inNam) {
      if (!(sym = symNew(d + (c ? 0 : 1), i - (c ? 0 : 1)))
       || !(cv = symsAdd(V->syms, sym))) {
        symFre(sym);
        free(d);
        fprintf(stderr, "symNew/symsAdd fail @%s:%u:%u(%.*s)\n", V->fil, r, c, l->l, l->s);
        return (1);
      }
      free(d);
      if (cv != sym) {
        symFre(sym);
        sym = cv;
      }
      if (!(nam = namNew(sym))
       || !(cv = namsAdd(V->nams, nam))) {
        namFre(nam);
        fprintf(stderr, "namNew/namsAdd fail @%s:%u:%u(%.*s)\n", V->fil, r, c, l->l, l->s);
        return (1);
      }
      if (cv != nam) {
        namFre(nam);
        nam = cv;
      }
      if (!(tv = realloc(V->col, (V->coln + 1) * sizeof (*V->col)))) {
        fprintf(stderr, "realloc fail @%s:%u:%u(%.*s)\n", V->fil, r, c, l->l, l->s);
        return (1);
      }
      V->col = tv;
      for (j = 0; j < V->coln && nam != *(V->col + j); ++j);
      if (j < V->coln) {
        fprintf(stderr, "duplicate name @%s:%u:%u(%.*s)\n", V->fil, r, c, l->l, l->s);
        return (1);
      }
      *(V->col + V->coln++) = nam;
      break;
    }
    if (c > V->coln) {
      fprintf(stderr, "excess CSValue @%s:%u:%u(%.*s)\n", V->fil, r, c, l->l, l->s);
      free(d);
      return (1);
    }
    if (!(sym = symNew(d, i))
     || !(cv = symsAdd(V->syms, sym))) {
      symFre(sym);
      free(d);
      fprintf(stderr, "symNew/symsAdd fail @%s:%u:%u(%.*s)\n", V->fil, r, c, l->l, l->s);
      return (1);
    }
    free(d);
    if (cv != sym) {
      symFre(sym);
      sym = cv;
    }
    if (!(val = valNew(*(V->col + c), sym))
     || !(cv = valsAdd((*(V->col + c))->vals, val))) {
      valFre(val);
      fprintf(stderr, "valNew/valsAdd fail @%s:%u:%u ,%.*s,\n", V->fil, r, c, l->l, l->s);
      return (1);
    }
    if (cv != val) {
      valFre(val);
      val = cv;
    }
    if (!c) {
      if (!(V->inf = infNew(val, V->fil, r))) {
        fprintf(stderr, "infNew fail @%s:%u:%u(%.*s)\n", V->fil, r, c, l->l, l->s);
        return (1);
      }
    } else {
      if (!(cv = valsAdd(V->inf->vals, val))) {
        fprintf(stderr, "valsAdd fail @%s:%u:%u(%.*s)\n", V->fil, r, c, l->l, l->s);
        return (1);
      }
      if (cv != val) {
        valFre(val);
        fprintf(stderr, "duplicate val @%s:%u:%u(%.*s)\n", V->fil, r, c, l->l, l->s);
        return (1);
      }
    }
    break;
  }
  return (0);
#undef V
}

static void
csvFre(
  struct csv *v
){
  if (!v)
    return;
  free(v->col);
  infFre(v->inf);
  infsFre(v->infs);
  namsFre(v->nams);
  symsFre(v->syms);
  free(v);
}

static void
csvPrt(
  const unsigned char *v
 ,unsigned char n
){
  unsigned char *e;
  int l;

  if (!(e = malloc(n * 2))
   || (l = csvEncodeValue(e, n * 2, v, n)) <= 0)
    printf("%.*s", (int)n, v);
  else
    printf("%.*s", l, e);
  free(e);
}

/********************************************************************************/

/* single dependency transitive closure */
static int
infsValTrnAdd(
  const val_t *val
 ,const infs_t *infs
 ,infs_t *r
){
  vals_t *v1;
  vals_t *v2;
  vals_t *t;
  unsigned int i;
  unsigned int j;

  v2 = 0;
  if (!(v1 = valsNew())
   || !(v2 = valsNew())
   || !(valsAdd(v1, val))) {
    valsRefFre(v2);
    valsRefFre(v1);
    return (1);
  }
  for (;;) {
    for (i = 0; i < v1->n; ++i)
      for (j = 0; j < infs->n; ++j)
        if ((*(infs->v + j))->vals->n == 1
         && *(*(infs->v + j))->vals->v == *(v1->v + i)
         && (!infsAdd(r, *(infs->v + j)) || !valsAdd(v2, (*(infs->v + j))->val))) {
          valsRefFre(v2);
          valsRefFre(v1);
          infsRefFre(r);
          return (1);
        }
    if (!v2->n)
      break;
    t = v1;
    v1 = v2;
    v2 = t;
    v2->n = 0;
  }
  valsRefFre(v2);
  valsRefFre(v1);
  return (0);
}

static int
infsVal(
  const val_t *val
 ,const infs_t *infs
 ,infs_t *r
){
  vals_t *v;
  unsigned int i;
  unsigned int j;

  v = 0;
  if (!(v = valsNew())
   || !(valsAdd(v, val))) {
    valsRefFre(v);
    return (1);
  }
  for (i = 0; i < v->n; ++i)
    for (j = 0; j < infs->n; ++j)
      if (bsearch(*(v->v + i)
          ,(*(infs->v + j))->vals->v
          ,(*(infs->v + j))->vals->n
          ,sizeof (*(*(infs->v + j))->vals->v)
          ,(int(*)(const void *, const void *))valsSchCmp
          )
       && !infsAdd(r, *(infs->v + j))) {
        valsRefFre(v);
        infsRefFre(r);
        return (1);
      }
  valsRefFre(v);
  return (0);
}

/* independent values */
static vals_t *
namsInd(
  const nams_t *nams
 ,const infs_t *infs
){
  vals_t *r;
  unsigned int i;
  unsigned int j;

  if (!(r = valsNew()))
    return (0);
  for (i = 0; i < nams->n; ++i)
    for (j = 0; j < (*(nams->v + i))->vals->n; ++j) {
      if (!bsearch(*((*(nams->v + i))->vals->v + j)
          ,infs->v
          ,infs->n
          ,sizeof (*infs->v)
          ,(int(*)(const void *, const void *))infsValSchCmp
          )
       && valsAdd(r, *((*(nams->v + i))->vals->v + j)) != *((*(nams->v + i))->vals->v + j)) {
          valsRefFre(r);
          return (0);
      }
    }
  for (i = 0; i < r->n; ++i) {
    if (!(*((infs_t **)&(*(r->v + i))->infs) = infsNew())
     || infsVal(*(r->v + i), infs, (*(r->v + i))->infs)) {
      valsRefFre(r);
      return (0);
    }
  }
  return (r);
}

static int
valsInfsCmp(
  const val_t **e1
 ,const val_t **e2
){
  unsigned int i1;
  unsigned int i2;
  unsigned int o1;
  unsigned int o2;
  unsigned int i;

  /* find sum of "other" infs */
  for (o1 = i = 0; i < (*e1)->nam->vals->n; ++i)
    if (*((*e1)->nam->vals->v + i) != *e1)
      o1 += (*((*e1)->nam->vals->v + i))->infs->n;
  for (o2 = i = 0; i < (*e2)->nam->vals->n; ++i)
    if (*((*e2)->nam->vals->v + i) != *e2)
      o2 += (*((*e2)->nam->vals->v + i))->infs->n;
  /* primary sort balance (smallest difference of number of inf_t) */
  i1 = (*e1)->infs->n > o1 ? (*e1)->infs->n - o1 : o1 - (*e1)->infs->n;
  i2 = (*e2)->infs->n > o2 ? (*e2)->infs->n - o2 : o2 - (*e2)->infs->n;
  if (i1 < i2)
    return (-1);
  if (i1 > i2)
    return (1);
  /* secondary sort delay (largest smallest number of inf_t) */
  i1 = (*e1)->infs->n > o1 ? o1 : (*e1)->infs->n;
  i2 = (*e2)->infs->n > o2 ? o2 : (*e2)->infs->n;
  if (i1 > i2)
    return (-1);
  if (i1 < i2)
    return (1);
  return (0);
}

static vals_t *
valsSubValNam(
  const vals_t *vals
 ,const val_t *val
 ,const infs_t *infs
){
  vals_t *r;
  unsigned int i;
  unsigned int j;

  r = 0;
  if (!vals || !val
   || !(r = valsNew())
   || !(r->v = malloc(vals->n * sizeof (*r->v)))) {
    valsRefFre(r);
    return (0);
  }
  for (r->n = i = 0; i < vals->n; ++i)
    if ((*(vals->v + i))->nam != val->nam) {
      for (j = 0; j < infs->n; ++j)
        if (bsearch(*(vals->v + i)
           ,(*(infs->v + j))->vals->v
           ,(*(infs->v + j))->vals->n
           ,sizeof (*(*(infs->v + j))->vals->v)
           ,(int(*)(const void *, const void *))valsSchCmp))
          break;
      if (j < infs->n)
        *(r->v + r->n++) = *(vals->v + i);
    }
  return (r);
}

static vals_t *
valsSubVal(
  const vals_t *vals
 ,const val_t *val
 ,const infs_t *infs
){
  vals_t *r;
  unsigned int i;
  unsigned int j;
  unsigned int k;

  r = 0;
  if (!vals || !val
   || !(r = valsNew())
   || !(r->v = malloc(vals->n * sizeof (*r->v)))) {
    valsRefFre(r);
    return (0);
  }
  for (r->n = j = i = 0; i < vals->n; ++i)
    if (*(vals->v + i) != val) {
      for (k = 0; k < infs->n; ++k)
        if (bsearch(*(vals->v + i)
           ,(*(infs->v + k))->vals->v
           ,(*(infs->v + k))->vals->n
           ,sizeof (*(*(infs->v + k))->vals->v)
           ,(int(*)(const void *, const void *))valsSchCmp))
          break;
      if (k < infs->n) {
        *(r->v + r->n++) = *(vals->v + i);
        if ((*(vals->v + i))->nam == val->nam)
          ++j;
      }
    }
  if (j == 1) {
    /* filter r in place instead of rebuilding with valsSubValNam */
    for (j = i = 0; i < r->n; ++i)
      if ((*(r->v + i))->nam != val->nam)
        *(r->v + j++) = *(r->v + i);
    r->n = j;
  }
  return (r);
}

/* infs resolved by this val and not transitivly dependent on the remaining vals */
static infs_t *
infsResVal(
  const vals_t *vals
 ,const infs_t *infs
 ,const val_t *val
){
  infs_t *r;
  unsigned int i;
  unsigned int j;
  unsigned int k;
  unsigned int m;
  unsigned int n;
  int c;

  if (!(r = infsNew())
   || !(r->v = malloc((infs->n < val->infs->n ? infs->n : val->infs->n) * sizeof (*r->v))))
    return (0);
  i = j = 0;
  while (i < infs->n && j < val->infs->n) {
    c = infCmp(*(infs->v + i), *(val->infs->v + j));
    if (c < 0)
      ++i;
    else if (c > 0)
      ++j;
    else {
      for (k = 0; k < (*(infs->v + i))->vals->n; ++k)
        if (*((*(infs->v + i))->vals->v + k) != val) {
          if (bsearch(*((*(infs->v + i))->vals->v + k)
              ,vals->v
              ,vals->n
              ,sizeof (*vals->v)
              ,(int(*)(const void *, const void *))valsSchCmp
              )
          )
            break;
          for (m = 0; m < infs->n; ++m) {
            if (*((*(infs->v + i))->vals->v + k) != (*(infs->v + m))->val)
              continue;
            for (n = 0; n < (*(infs->v + m))->vals->n; ++n) {
              if (bsearch(*((*(infs->v + m))->vals->v + n)
                  ,vals->v
                  ,vals->n
                  ,sizeof (*vals->v)
                  ,(int(*)(const void *, const void *))valsSchCmp
                  )
              )
                break;
            }
            if (n < (*(infs->v + m))->vals->n)
              break;
          }
          if (m < infs->n)
            break;
        }
      if (k == (*(infs->v + i))->vals->n)
        *(r->v + r->n++) = *(infs->v + i);
      ++i;
      ++j;
    }
  }
  return (r);
}

/* infs resolved by this val's nam's vals and not transitivly dependent on the remaining vals */
static infs_t *
infsResValNam(
  const vals_t *vals
 ,const infs_t *infs
 ,const val_t *val
){
  infs_t *r;
  infs_t *t;
  unsigned int i;

  r = 0;
  for (i = 0; i < val->nam->vals->n; ++i) {
    if (*(val->nam->vals->v + i) == val)
      continue;
    if (!bsearch(*(val->nam->vals->v + i)
        ,vals->v
        ,vals->n
        ,sizeof (*vals->v)
        ,(int(*)(const void *, const void *))valsSchCmp))
      continue;
    t = infsResVal(vals, r ? r : infs, *(val->nam->vals->v + i));
    infsRefFre(r);
    if (!t)
      return (t);
    r = t;
  }
  if (r)
    return (r);
  else
    return (infsNew());
}

/* Minus */
static infs_t *
infsMnsInfs(
  const infs_t *infs1
 ,const infs_t *infs2
){
  infs_t *r;
  unsigned int i;
  unsigned int j;
  int c;

  if (!(r = infsNew())
   || !(r->v = malloc(infs1->n * sizeof (*r->v))))
    return (0);
  i = j = 0;
  while (i < infs1->n && j < infs2->n) {
    c = infCmp(*(infs1->v + i), *(infs2->v + j));
    if (c < 0) {
      *(r->v + r->n++) = *(infs1->v + i);
      ++i;
    } else if (c > 0) {
      ++j;
    } else {
      ++i;
      ++j;
    }
  }
  for (; i < infs1->n; ++i)
    *(r->v + r->n++) = *(infs1->v + i);
  return (r);
}

/* Strip inf with same val or other vals */
static infs_t *
infsSrpInfs(
  const infs_t *infs1
 ,const infs_t *infs2
){
  infs_t *r;
  unsigned int i;
  unsigned int j;
  unsigned int k;

  if (!(r = infsNew())
   || !(r->v = malloc(infs1->n * sizeof (*r->v))))
    return (0);
  for (i = 0; i < infs1->n; ++i) {
    if (bsearch((*(infs1->v + i))->val
        ,infs2->v
        ,infs2->n
        ,sizeof (*infs2->v)
        ,(int(*)(const void *, const void *))infsValSchCmp
        )
    )
      continue;
    for (j = 0; j < (*(infs1->v + i))->vals->n; ++j) {
      for (k = 0; k < infs2->n; ++k) {
        if (*((*(infs1->v + i))->vals->v + j) != (*(infs2->v + k))->val
         && (*((*(infs1->v + i))->vals->v + j))->nam == (*(infs2->v + k))->val->nam)
          break;
      }
      if (k < infs2->n)
        break;
    }
    if (j == (*(infs1->v + i))->vals->n)
      *(r->v + r->n++) = *(infs1->v + i);
  }
  return (r);
}

/********************************************************************************/

typedef struct nod nod_t;

struct nod {
  const val_t *val;
  infs_t *infsV;
  infs_t *infsO;
  const nod_t *nodV;
  const nod_t *nodO;
  unsigned int d;
  unsigned int lbl;
};

static nod_t *
nodNew(
  void
){
  return (calloc(1, sizeof (nod_t)));
}

#if DTC_DEBUG
static void
nodPrt(
  const nod_t *nod
){
  if (!nod)
    return;
  printf("nod %d (\n", nod->d);
  valPrt(nod->val);
  infsPrt(nod->infsV);
  infsPrt(nod->infsO);
  nodPrt(nod->nodV);
  nodPrt(nod->nodO);
  puts(")");
}

static void
nodNstPrt(
  const nod_t *nod
 ,unsigned int d
){
  unsigned int i;
  unsigned int s;

  if (!nod)
    return;
  for (s = 0; s < d; ++s)putchar(' '),putchar(' ');
  if (nod->val)
    printf("{ %.*s %.*s\n", nod->val->nam->sym->n, nod->val->nam->sym->v, nod->val->sym->n, nod->val->sym->v);
  else
    puts("{ !val");
  if (nod->infsV) for (i = 0; i < nod->infsV->n; ++i) {
    for (s = 0; s < d; ++s)putchar(' '),putchar(' '),putchar(' '),putchar(' ');
    printf("= %.*s %.*s\n", (*(nod->infsV->v + i))->val->nam->sym->n, (*(nod->infsV->v + i))->val->nam->sym->v, (*(nod->infsV->v + i))->val->sym->n, (*(nod->infsV->v + i))->val->sym->v);
  }
  nodNstPrt(nod->nodV, d + 1);
  for (s = 0; s < d; ++s)putchar(' '),putchar(' ');
  puts("}{");
  if (nod->infsO) for (i = 0; i < nod->infsO->n; ++i) {
    for (s = 0; s < d; ++s)putchar(' '),putchar(' '),putchar(' '),putchar(' ');
    printf("= %.*s %.*s\n", (*(nod->infsO->v + i))->val->nam->sym->n, (*(nod->infsO->v + i))->val->nam->sym->v, (*(nod->infsO->v + i))->val->sym->n, (*(nod->infsO->v + i))->val->sym->v);
  }
  nodNstPrt(nod->nodO, d + 1);
  for (s = 0; s < d; ++s)putchar(' '),putchar(' ');
  puts("}");
}

static void
nodValsPrt(
  const vals_t *vals
){
  unsigned int i;
  unsigned int j;

  if (!vals)
    return;
  for (i = 0; i < vals->n; ++i) {
    printf(" %.*s %.*s\n"
    ,(*(vals->v + i))->nam->sym->n
    ,(*(vals->v + i))->nam->sym->v
    ,(*(vals->v + i))->sym->n
    ,(*(vals->v + i))->sym->v
    );
    for (j = 0; j < (*(vals->v + i))->infs->n; ++j)
      printf("  = %.*s %.*s\n"
      ,(*((*(vals->v + i))->infs->v + j))->val->nam->sym->n
      ,(*((*(vals->v + i))->infs->v + j))->val->nam->sym->v
      ,(*((*(vals->v + i))->infs->v + j))->val->sym->n
      ,(*((*(vals->v + i))->infs->v + j))->val->sym->v
      );
  }
}

static void
nodInfsPrt(
  const infs_t *infs
){
  unsigned int i;
  unsigned int j;

  if (!infs)
    return;
  for (i = 0; i < infs->n; ++i) {
    printf(" = %.*s %.*s"
    ,(*(infs->v + i))->val->nam->sym->n
    ,(*(infs->v + i))->val->nam->sym->v
    ,(*(infs->v + i))->val->sym->n
    ,(*(infs->v + i))->val->sym->v
    );
    for (j = 0; j < (*(infs->v + i))->vals->n; ++j)
      printf(" : %.*s %.*s"
      ,(*((*(infs->v + i))->vals->v + j))->nam->sym->n
      ,(*((*(infs->v + i))->vals->v + j))->nam->sym->v
      ,(*((*(infs->v + i))->vals->v + j))->sym->n
      ,(*((*(infs->v + i))->vals->v + j))->sym->v
      );
    putchar('\n');
  }
}
#endif

static int
infsChk(
  const infs_t *infs
 ,const char *prg
){
  unsigned int i;
  unsigned int j;
  int r;

  r = 0;
  if (!infs)
    return (r);
  for (i = 0; i < infs->n; ++i)
    for (j = i + 1; j < infs->n; ++j)
      if ((*(infs->v + i))->val->nam == (*(infs->v + j))->val->nam
       && (*(infs->v + i))->val != (*(infs->v + j))->val) {
        fprintf(stderr, "%s: unresolvable \"%.*s\": \"%.*s\" @%s:%u vs \"%.*s\" @%s:%u\n"
        ,prg
        ,(*(infs->v + i))->val->nam->sym->n
        ,(*(infs->v + i))->val->nam->sym->v
        ,(*(infs->v + i))->val->sym->n
        ,(*(infs->v + i))->val->sym->v
        ,(*(infs->v + i))->fil
        ,(*(infs->v + i))->row
        ,(*(infs->v + j))->val->sym->n
        ,(*(infs->v + j))->val->sym->v
        ,(*(infs->v + j))->fil
        ,(*(infs->v + j))->row
        );
        r = 1;
      }
  return (r);
}

static int
nodChk(
  const nod_t *nod
 ,const char *prg
){
  int r;

  r = 0;
  if (!nod)
    return (r);
  r |= infsChk(nod->infsV, prg);
  r |= infsChk(nod->infsO, prg);
  r |= nodChk(nod->nodV, prg);
  r |= nodChk(nod->nodO, prg);
  return (r);
}

static void
nodFre(
  nod_t *nod
){
  if (!nod)
    return;
  infsRefFre(nod->infsV);
  infsRefFre(nod->infsO);
  free(nod);
}

/********************************************************************************/

typedef struct bld bld_t;

struct bld {
  vals_t *vals;
  infs_t *infs;
  nod_t *nod;
};

static bld_t *
bldNew(
  const vals_t *vals
 ,const infs_t *infs
){
  bld_t *r;

  if (!vals || !infs
   || !(r = calloc(1, sizeof (*r))))
    return (0);
  if (!(r->vals = valsRefDup(vals))
   || !(r->infs = infsRefDup(infs))) {
    valsRefFre(r->vals);
    free(r);
    return (0);
  }
  return (r);
}

#if DTC_DEBUG
static void
bldPrt(
  const bld_t *bld
){
  if (!bld)
    return;
  valsPrt(bld->vals);
  infsPrt(bld->infs);
  nodPrt(bld->nod);
}
#endif

static void
bldFre(
  bld_t *bld
){
  if (!bld)
    return;
  valsRefFre(bld->vals);
  infsRefFre(bld->infs);
  nodFre(bld->nod);
  free(bld);
}

static int
bldCmp(
  const bld_t *e1
 ,const bld_t *e2
){
  int r;

  if ((r = valsCmp(e1->vals, e2->vals)))
    return (r);
  return (infsCmp(e1->infs, e2->infs));
}

/********************************************************************************/

typedef struct blds blds_t;

struct blds {
  bld_t **v;
  unsigned int n;
};

static blds_t *
bldsNew(
  void
){
  return (calloc(1, sizeof (blds_t)));
}

static int
bldsSchCmp(
  const bld_t *k
 ,const bld_t **e
){
  return (bldCmp(k, *e));
}

static int
bldsSrtCmp(
  const bld_t **e1
 ,const bld_t **e2
){
  return (bldCmp(*e1, *e2));
}

static bld_t *
bldsFnd(
  blds_t *blds
 ,const vals_t *vals
 ,const infs_t *infs
){
  bld_t **r;
  bld_t k;

  if (!blds || !vals || !infs)
    return (0);
  k.vals = (vals_t *)vals;
  k.infs = (infs_t *)infs;
  if ((r = bsearch(&k, blds->v, blds->n, sizeof (*blds->v), (int(*)(const void *, const void *))bldsSchCmp)))
    return (*r);
  return (0);
}

static const bld_t *
bldsAdd(
  blds_t *blds
 ,bld_t *bld
){
  void *v;
  unsigned int lo;
  unsigned int hi;
  unsigned int mid;
  int c;

  if (!bld || !blds)
    return (0);
  /* binary search for insertion position - O(n) vs qsort O(n log n) */
  lo = 0;
  hi = blds->n;
  while (lo < hi) {
    mid = lo + (hi - lo) / 2;
    c = bldsSrtCmp((const bld_t **)&bld, (const bld_t **)(blds->v + mid));
    if (c < 0)
      hi = mid;
    else
      lo = mid + 1;
  }
  if (!(v = realloc(blds->v, (blds->n + 1) * sizeof (*blds->v))))
    return (0);
  blds->v = v;
  if (lo < blds->n)
    memmove(blds->v + lo + 1, blds->v + lo, (blds->n - lo) * sizeof (*blds->v));
  *(blds->v + lo) = bld;
  ++blds->n;
  return (bld);
#if 0 /* original qsort technique - O(n log n) per insert */
  if (!bld || !blds)
    return (0);
  if (!(v = realloc(blds->v, (blds->n + 1) * sizeof (*blds->v))))
    return (0);
  blds->v = v;
  *(blds->v + blds->n++) = bld;
  qsort(blds->v, blds->n, sizeof (*blds->v), (int(*)(const void *, const void *))bldsSrtCmp);
  return (bld);
#endif
}

#if DTC_DEBUG
static void
bldsPrt(
  const blds_t *blds
){
  unsigned int i;

  if (!blds)
    return;
  puts("blds(");
  for (i = 0; i < blds->n; ++i)
    bldPrt(*(blds->v + i));
  puts(")");
}
#endif

static void
bldsFre(
  blds_t *blds
){
  if (!blds)
    return;
  while (blds->n--)
    bldFre(*(blds->v + blds->n));
  free(blds->v);
  free(blds);
}

static const nod_t *
nodBld(
  blds_t *blds
 ,const vals_t *vals
 ,const infs_t *infs
 ,unsigned int bd
 ,unsigned int q
){
  bld_t *bld;
  vals_t *vs;
  nod_t *r;
  infs_t *nV;
  infs_t *nO;
  vals_t *fV;
  vals_t *fO;
  infs_t *d;
  unsigned int i;
  unsigned int j;

#if DTC_DEBUG
printf("B %u %u %u\n",vals->n,infs->n,bd);
nodValsPrt(vals);
nodInfsPrt(infs);
#endif
  if ((bld = bldsFnd(blds, vals, infs))) {
#if DTC_DEBUG
    printf("cache %s %u\n", bld->nod->val ? "val" : "!val", bld->nod->d);
#endif
    return (bld->nod);
  }
  if (!(bld = bldNew(vals, infs))
   || !(bld->nod = nodNew())
   || !(vs = valsRefDup(vals))) {
    bldFre(bld);
    return (0);
  }
  qsort(vs->v, vs->n, sizeof (*vs->v), (int(*)(const void *, const void *))valsInfsCmp);

  for (i = 0; i < vs->n; ++i) {
#if DTC_DEBUG
printf("I %u %.*s %.*s\n",i,(*(vs->v + i))->nam->sym->n,(*(vs->v + i))->nam->sym->v,(*(vs->v + i))->sym->n,(*(vs->v + i))->sym->v);
nodInfsPrt((*(vs->v + i))->infs);
#endif

    if (!(r = nodNew()))
      goto error3;

    nO = 0;
    if (!(nV = infsResVal(vals, infs, *(vs->v + i)))
     || !(nO = infsResValNam(vals, infs, *(vs->v + i))))
      goto error2;

#if DTC_DEBUG
puts("infsV");
nodInfsPrt(nV);
puts("infsO");
nodInfsPrt(nO);
#endif

    if (nV->n) {
      for (j = 0; j < nV->n; ++j)
        if (infsValTrnAdd((*(nV->v + j))->val, infs, nV))
          goto error2;
      r->infsV = nV;
#if DTC_DEBUG
puts("infsV");
nodInfsPrt(r->infsV);
#endif
    } else
      infsRefFre(nV);
    nV = 0;

    if (nO->n) {
      for (j = 0; j < nO->n; ++j)
        if (infsValTrnAdd((*(nO->v + j))->val, infs, nO))
          goto error2;
      r->infsO = nO;
#if DTC_DEBUG
puts("infsO");
nodInfsPrt(r->infsO);
#endif
    } else
      infsRefFre(nO);
    nO = 0;

    for (d = 0, j = 0; j < (*(vs->v + i))->nam->vals->n; ++j) {
      if (*((*(vs->v + i))->nam->vals->v + j) == *(vs->v + i))
        continue;
      if (!(nV = infsMnsInfs(d ? d : infs, (*((*(vs->v + i))->nam->vals->v + j))->infs))) {
        infsRefFre(d);
        goto error2;
      }
      infsRefFre(d);
      d = nV;
    }
    if (!nV)
      goto error2;

    if (!(nO = infsMnsInfs(infs, (*(vs->v + i))->infs)))
      goto error2;

#if DTC_DEBUG
puts("nV");
nodInfsPrt(nV);
puts("nO");
nodInfsPrt(nO);
#endif

    if (nV->n && r->infsV) {
      d = infsSrpInfs(nV, r->infsV);
      if (!d)
        goto error2;
      infsRefFre(nV);
      nV = d;
#if DTC_DEBUG
puts("nV");
nodInfsPrt(nV);
#endif
    }

    if (nO->n && r->infsO) {
      d = infsSrpInfs(nO, r->infsO);
      if (!d)
        goto error2;
      infsRefFre(nO);
      nO = d;
#if DTC_DEBUG
puts("nO");
nodInfsPrt(nO);
#endif
    }

    fV = fO = 0;
    if (nV->n && !(fV = valsSubValNam(vals, *(vs->v + i), nV)))
      goto error1;
    if (nO->n && !(fO = valsSubVal(vals, *(vs->v + i), nO)))
      goto error1;

#if DTC_DEBUG
puts("fV");
nodValsPrt(fV);
puts("fO");
nodValsPrt(fO);
#endif

    if ((fV && !fV->n)
     || (fO && !fO->n)) {
#if DTC_DEBUG
puts("!fV || !fO");
#endif
      valsRefFre(fO);
      valsRefFre(fV);
      infsRefFre(nV);
      infsRefFre(nO);
      nodFre(r);
      continue;
    }

    r->val = *(vs->v + i);

#if DTC_DEBUG
puts("V");
#endif
    if (fV && !(r->nodV = nodBld(blds, fV, nV, bd, q)))
      goto error1;

#if DTC_DEBUG
puts("O");
#endif
    if (fO && !(r->nodO = nodBld(blds, fO, nO, bd, q)))
      goto error1;

    valsRefFre(fO);
    valsRefFre(fV);
    infsRefFre(nO);
    infsRefFre(nV);

    if (r->nodV || r->nodO) {
      if (r->nodV && r->nodO && r->nodV->val && r->nodO->val)
        r->d = 1 + (r->nodV->d > r->nodO->d ? r->nodV->d : r->nodO->d);
      else if (!r->nodO && r->nodV && r->nodV->val)
        r->d = 1 + r->nodV->d;
      else if (!r->nodV && r->nodO && r->nodO->val)
        r->d = 1 + r->nodO->d;
      else {
        nodFre(r);
        continue;
      }
    }
    if (r->d > bd) {
#if DTC_DEBUG
printf("not better %u > %u\n", r->d, bd);
#endif
      nodFre(r);
      continue;
    }
    if (!bld->nod->val || r->d < bld->nod->d) {
      nodFre(bld->nod);
      bld->nod = r;
      if (q || !bld->nod->d)
        break;
      bd = bld->nod->d;
    } else
      nodFre(r);
  }
  if (!bld->nod->val) {
    if (!(bld->nod->infsV = infsRefDup(infs)))
      goto error3;
#if DTC_DEBUG
puts("!val");
nodInfsPrt(infs);
#endif
  }
  if (!bldsAdd(blds, bld))
    goto error3;
  valsRefFre(vs);
  return (bld->nod);
error1:
  valsRefFre(fO);
  valsRefFre(fV);
error2:
  infsRefFre(nO);
  infsRefFre(nV);
  nodFre(r);
error3:
  valsRefFre(vs);
  bldFre(bld);
  return (0);
}

/********************************************************************************/

/* output state */
typedef struct out out_t;

struct out {
  struct {
    const infs_t *i;
    const nod_t *n;
    unsigned int l;
  } *b;
  unsigned int n;
  unsigned int l;
};

/* compare infs by result values */
static int
outCmp(
  const infs_t *a
 ,const infs_t *b
){
  unsigned int i;

  if (!a && !b)
    return (0);
  if (!a || !b || a->n != b->n)
    return (1);
  for (i = 0; i < a->n; ++i)
    if (valCmp((*(a->v + i))->val, (*(b->v + i))->val))
      return (1);
  return (0);
}

static void outNod(out_t *, const nod_t *);

/* find or reserve label for branch, set *dup if duplicate */
static unsigned int
outBrnLbl(
  out_t *out
 ,const infs_t *infs
 ,const nod_t *nod
 ,int *dup
){
  unsigned int i;
  void *v;

  for (i = 0; i < out->n; ++i)
    if ((out->b + i)->n == nod && !outCmp((out->b + i)->i, infs)) {
      *dup = 1;
      return ((out->b + i)->l);
    }
  *dup = 0;
  i = out->l++;
  if ((v = realloc(out->b, (out->n + 1) * sizeof (*out->b)))) {
    out->b = v;
    (out->b + out->n)->i = infs;
    (out->b + out->n)->n = nod;
    (out->b + out->n)->l = i;
    ++out->n;
  }
  return (i);
}

/* output branch content */
static void
outBrnCon(
  out_t *out
 ,const infs_t *infs
 ,const nod_t *nod
){
  unsigned int i;

  if (infs)
    for (i = 0; i < infs->n; ++i) {
      printf("R,");
      csvPrt((*(infs->v + i))->val->nam->sym->v, (*(infs->v + i))->val->nam->sym->n);
      putchar(',');
      csvPrt((*(infs->v + i))->val->sym->v, (*(infs->v + i))->val->sym->n);
      putchar('\n');
    }
  if (nod)
    outNod(out, nod);
  else
    puts("J,0");
}

/* output branch with deduplication */
static void
outBrn(
  out_t *out
 ,const infs_t *infs
 ,const nod_t *nod
){
  unsigned int l;
  int dup;

  l = outBrnLbl(out, infs, nod, &dup);
  if (dup) {
    printf("J,%u\n", l);
  } else {
    printf("L,%u\n", l);
    outBrnCon(out, infs, nod);
  }
}

/* output node */
static void
outNod(
  out_t *out
 ,const nod_t *nod
){
  unsigned int i;
  unsigned int l;
  int dup;

  if (!nod)
    return;
  if (nod->lbl) {
    printf("J,%u\n", nod->lbl);
    return;
  }
  ((nod_t *)nod)->lbl = out->l;
  if (!nod->val) {
    for (i = 0; nod->infsV && i < nod->infsV->n; ++i) {
      printf("R,");
      csvPrt((*(nod->infsV->v + i))->val->nam->sym->v, (*(nod->infsV->v + i))->val->nam->sym->n);
      putchar(',');
      csvPrt((*(nod->infsV->v + i))->val->sym->v, (*(nod->infsV->v + i))->val->sym->n);
      putchar('\n');
    }
    return;
  }
  l = outBrnLbl(out, nod->infsV, nod->nodV, &dup);
  printf("T,");
  csvPrt(nod->val->nam->sym->v, nod->val->nam->sym->n);
  putchar(',');
  csvPrt(nod->val->sym->v, nod->val->sym->n);
  printf(",%u\n", l);
  outBrn(out, nod->infsO, nod->nodO);
  if (!dup) {
    printf("L,%u\n", l);
    outBrnCon(out, nod->infsV, nod->nodV);
  }
}

/********************************************************************************/

int
main(
  int argc
 ,char *argv[]
){
  struct csv *csv;
  vals_t *vals;
  blds_t *blds;
  const nod_t *nod;
  unsigned int i;
  unsigned int b;
  unsigned int q;

  if ((argc > 1 && !strcmp(argv[1], "-q") && argc < 3) || argc < 2) {
    fprintf(stderr, "Usage: %s [-] file ...\n", argv[0]);
    return (1);
  }
  vals = 0;
  blds = 0;
  if (!(csv = calloc(1, sizeof (*csv)))
   || !(csv->syms = symsNew())
   || !(csv->nams = namsNew())
   || !(csv->infs = infsNew())) {
    fprintf(stderr, "%s: alloc fail\n", argv[0]);
    goto exit;
  }
  if (!strcmp(argv[1], "-q"))
    q = 1, i = 2;
  else
    q = 0, i = 1;
  for (; i < (unsigned int)argc; ++i) {
    unsigned char *bf;
    int fd;
    int sz;

    if ((fd = open(argv[i], O_RDONLY)) < 0) {
      fprintf(stderr, "%s: Can't open %s\n", argv[0], argv[i]);
      goto exit;
    }
    bf = 0;
    if ((sz = lseek(fd, 0, SEEK_END)) < 0
     || lseek(fd, 0, SEEK_SET) < 0
     || !(bf = malloc(sz))
     || read(fd, bf, sz) != sz) {
      fprintf(stderr, "%s: data fail on %s\n", argv[0], argv[i]);
      free(bf);
      close(fd);
      goto exit;
    }
    close(fd);
    csv->fil = (unsigned char *)argv[i];
    if (csvParse(csvCb, bf, sz, csv) != sz) {
      fprintf(stderr, "%s: CSV parse fail on %s\n", argv[0], argv[i]);
      free(bf);
      goto exit;
    }
    free(bf);
  }
#if DTC_DEBUG
  infsPrt(csv->infs);
  putchar('\n');
  namsPrt(csv->nams);
  putchar('\n');
  symsPrt(csv->syms);
  putchar('\n');
#endif

  fprintf(stderr, "%s: Names: %u\n", argv[0], csv->nams->n);
  fprintf(stderr, "%s: Inferences: %u\n", argv[0], csv->infs->n);
  b = 0;
  for (i = 0; i < csv->nams->n; ++i) {
    if ((*(csv->nams->v + i))->vals->n < 2) {
      fprintf(stderr, "%s: Name %.*s has fewer than two values\n", argv[0]
      ,(*(csv->nams->v + i))->sym->n
      ,(*(csv->nams->v + i))->sym->v
      );
      b = 1;
    }
  }
  for (i = 0; i < csv->infs->n; ++i) {
    if (!(*(csv->infs->v + i))->vals->n) {
      fprintf(stderr, "%s: File %s at row %u has no dependencies\n", argv[0]
      ,(*(csv->infs->v + i))->fil
      ,(*(csv->infs->v + i))->row
      );
      b = 1;
    }
  }
  if (b)
    goto exit;
  if (!(vals = namsInd(csv->nams, csv->infs)) || !vals->n) {
    fprintf(stderr, "%s: There are no independent values\n", argv[0]);
    goto exit;
  }
  for (i = 0; i < vals->n; ++i) {
    unsigned int j;

    for (j = 0; j < (*(vals->v + i))->nam->vals->n; ++j) {
      if (!(*((*(vals->v + i))->nam->vals->v + j))->infs) {
        fprintf(stderr, "%s: independent name %.*s has dependent value %.*s\n"
        ,argv[0]
        ,(*(vals->v + i))->nam->sym->n
        ,(*(vals->v + i))->nam->sym->v
        ,(*((*(vals->v + i))->nam->vals->v + j))->sym->n
        ,(*((*(vals->v + i))->nam->vals->v + j))->sym->v
        );
        b = 1;
      }
    }
  }
  if (b)
    goto exit;
  fprintf(stderr, "%s: Independent values: %u\n", argv[0], vals->n);
  for (i = 0; i < vals->n; ++i) {
    printf("I,");
    csvPrt((*(vals->v + i))->nam->sym->v, (*(vals->v + i))->nam->sym->n);
    putchar(',');
    csvPrt((*(vals->v + i))->sym->v, (*(vals->v + i))->sym->n);
    putchar('\n');
  }
  for (i = 0; i < csv->infs->n; ++i) {
    if (i && (*(csv->infs->v + i))->val == (*(csv->infs->v + i - 1))->val)
      continue;
    printf("O,");
    csvPrt((*(csv->infs->v + i))->val->nam->sym->v, (*(csv->infs->v + i))->val->nam->sym->n);
    putchar(',');
    csvPrt((*(csv->infs->v + i))->val->sym->v, (*(csv->infs->v + i))->val->sym->n);
    putchar('\n');
  }
  fflush(stdout);
  if (!(blds = bldsNew())
   || !(nod = nodBld(blds, vals, csv->infs, vals->n, q))) {
    fprintf(stderr, "%s: build failed (out of memory)\n", argv[0]);
    goto exit;
  }
#if DTC_DEBUG
  puts("\nblds\n");
  bldsPrt(blds);
  puts("\nnod\n");
  nodPrt(nod);
  puts("\nnst\n");
  nodNstPrt(nod, 0);
  puts("\nend\n");
#endif

  /* check for unresolvable inferences (same name with different values) */
  if (nodChk(nod, argv[0]))
    goto exit;

  /* output pseudocode */
  {
    out_t out;

    printf("D,%u\n", nod->d + 1);
    out.b = 0;
    out.n = 0;
    out.l = 1;
    outNod(&out, nod);
    puts("L,0");
    free(out.b);
  }

exit:
  bldsFre(blds);
  valsRefFre(vals);
  csvFre(csv);
  return (0);
}
