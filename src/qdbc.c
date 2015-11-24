/**
 *  -- KDB to MATLAB native bridge --
 *
 *  (c) 2014-2015 Dmitry Marienko <dmitry.ema@gmail.com>
 *
 *  History:
 *   - 04-sep-2014: first release
 *   - 05-sep-2014: keyed dictionaries support is added
 *   - 12-nov-2015: added Makefile for Windows
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <memory.h>
#include "mex.h"
#include "matrix.h"

#ifdef _WIN32 // Some stubs for Windows
  #define F_GETFL 3
  void __security_check_cookie(int i) {};
  int fcntl (intptr_t fd, int cmd, ...) { return 1; }
#endif //_WIN32

#define KXVER 3
#include "kx/k.h"
#define MATID    "kdbml"
#define MATLAB70 719529
#define KDB2000  10957

#define SCHK(v) v==nh ? mxGetNaN() : (v==wh || v==-wh ? mxGetInf() : v)
#define ICHK(v) v==ni ? mxGetNaN() : (v==wi || v==-wi ? mxGetInf() : v)
#define LCHK(v) v==nj ? mxGetNaN() : (v==wj || v==-wj ? mxGetInf() : v)
#define RCHK(v) v==nf ? mxGetNaN() : (v==wf || v==-wf ? mxGetInf() : v)

#define Q(e,s) {if(e) return mexErrMsgIdAndTxt(MATID,s),-1;}

double utmt(double f){return f + KDB2000 + MATLAB70;}           // matlab from kdb+ datetime
double utmtsp(double f){return f/8.64e13 + KDB2000 + MATLAB70;} // matlab from kdb+ datetimespan

/**
 * Make kdb value of given type from ordinary data
 */
K kValue(int t,void* v) {
  K r;
  switch (t) {
    case 1:  r = kb(*(I*)v); break; // bool
    case 4:  r = kg(*(I*)v); break; // byte
    case 5:  r = kh(*(I*)v); break; // short
    case 6:  r = ki(*(I*)v); break; // int
    case 7:  r = kj(*(J*)v); break; // long
    case 8:  r = ke(*(F*)v); break; // real
    case 9:  r = kf(*(F*)v); break; // real
    case 10: r = kc(*(I*)v); break; // char
    case 11: r = ks(*(S*)v); break; // symbol
    case 12: r = ktj(-KP, *(J*)v); break; // timestamp
    case 13: { r = ka(-KM); r->i = *(I*) v; } break; // month
    case 14: r = kd(*(I*)v); break; // date
    case 15: r = kz(*(F*)v); break; // datetime
    case 16: r = ktj(-KN,*(J*)v); break; // timespan
    case 17: { r = ka(-KU); r->i = *(I*)v; } break; // min ?
    case 18: { r = ka(-KV); r->i = *(I*)v; } break; // sec ?
    case 19: r = kt(*(J*)v); break; // time (msec) ?
  }
  return r;
}


/**
 * Process primitives
 */
mxArray* makeatom(K x) {
  mxArray* r = NULL;
  double* arr;

  switch (x->t) {
    //boolean       i.e. 0b
    case -1: r = mxCreateLogicalScalar(x->g); break;

    //byte          i.e. 0x00
    case -4: r = mxCreateDoubleScalar(x->g); break;

    //short         i.e. 0h
    case -5: r = mxCreateDoubleScalar(SCHK(x->h));  break;

    //int           i.e. 0
    case -6: r = mxCreateDoubleScalar(ICHK(x->i)); break;

    //long          i.e. 0j
    case -7: r = mxCreateDoubleScalar(LCHK(x->j)); break;

    //real          i.e. 0e
    case -8: r = mxCreateDoubleScalar(RCHK(x->e)); break;

    //float         i.e. 0.0
    case -9: r = mxCreateDoubleScalar(RCHK(x->f)); break;

    //char          i.e. "a"
    case -10: r = mxCreateString((char*)&(x->i)); break;

    //symbol        i.e. `abc
    case -11: r = mxCreateString(x->s); break;

    //timestamp     i.e. dateDtimespan
    case -12: r = mxCreateDoubleScalar(utmtsp(x->j)); break;

    //month         i.e. 2000.01m
    case -13: {
      int timy=(x->i)/12+2000;
      int timm=(x->i)%12+1;
      r = mxCreateDoubleMatrix((mwSize)1, (mwSize)2, mxREAL);
      arr = mxGetPr(r);
      arr[0] = timy; arr[1] = timm;  // here we return vector [yyyy mm]
    } break;

    //date          i.e. 2000.01.01
    case -14: r = mxCreateDoubleScalar(utmt(x->i)); break;

    //datetime  i.e. 2012.01.01T12:12:12.000
    case -15: r = mxCreateDoubleScalar(utmt(x->f)); break;

    //timespan      i.e. 0D00:00:00.000000000
    case -16: r = mxCreateDoubleScalar((double)x->j/8.64e13); break;

    //minute        i.e. 00:00
    case -17: r = mxCreateDoubleScalar((60.0*(double)x->i)/86400); break;

    //second        i.e. 00:00:00
    case -18: r = mxCreateDoubleScalar(((double)x->i)/86400); break;

    //time          i.e. 00:00:00.000
    case -19: r = mxCreateDoubleScalar(((double)x->i/1000)/86400); break;

    //default
    default: mexPrintf("cannot support %d type\n",x->t); break;
  }

  return r;
}

/**
 * Process list
 */
mxArray* makelist(K x) {
  K obj;
  mxArray* r = NULL;
  int i, nData = x->n;
  mwSize ndims[] = {(mwSize) nData};
  mxLogical* ll;
  double* dl;
  mxChar* cl;

  switch (x->t) {
    //mixed   i.e. 1 1b 0x01
    case 0:
      r = mxCreateCellArray((mwSize) 1, ndims);
      for(i=0;i<nData;i++)
        if (kK(x)[i]->t<0) mxSetCell(r, i, makeatom(kK(x)[i]));
        else mxSetCell(r, i, makelist(kK(x)[i]));
      break;

    //boolean       i.e. 01011b
    case 1:
      r = mxCreateLogicalArray((mwSize) 1, ndims);
      ll = mxGetLogicals(r);
      memcpy(ll, kG(x), nData * sizeof(bool));
    break;

    //byte    i.e. 0x0204
    case 4:
      r = mxCreateDoubleMatrix((mwSize) 1, nData, mxREAL);
      dl = mxGetPr(r);
      for(i=0;i<nData;i++) dl[i] = kG(x)[i];
    break;

    //short   i.e. 1 2h
    case 5:
      r = mxCreateDoubleMatrix((mwSize) 1, nData, mxREAL);
      dl = mxGetPr(r);
      for(i=0;i<nData;i++) dl[i] = SCHK(kH(x)[i]);
    break;

    //int   i.e. 1 2
    case 6:
      r = mxCreateDoubleMatrix((mwSize) 1, nData, mxREAL);
      dl = mxGetPr(r);
      for(i=0;i<nData;i++) dl[i] = ICHK(kI(x)[i]);
    break;

    //long    i.e. 1 2j
    case 7:
      r = mxCreateDoubleMatrix((mwSize) 1, nData, mxREAL);
      dl = mxGetPr(r);
      for(i=0;i<nData;i++) dl[i] = LCHK(kJ(x)[i]);
    break;

    //real    i.e. 1.0 2.0e
    case 8:
      r = mxCreateDoubleMatrix((mwSize) 1, nData, mxREAL);
      dl = mxGetPr(r);
      for(i=0;i<nData;i++) dl[i] = RCHK(kE(x)[i]);
    break;

    //float   i.e. 1.0 2.0
    case 9:
      r = mxCreateDoubleMatrix((mwSize) 1, nData, mxREAL);
      dl = mxGetPr(r);
      for(i=0;i<nData;i++) dl[i] = RCHK(kF(x)[i]);
    break;

    //char    i.e. "ab"
    case 10:
      r = mxCreateCharArray((mwSize) 1, ndims);
      cl = mxGetData(r);
      for(i=0;i<nData;i++) cl[i] = kC(x)[i];
    break;

    //symbol  i.e. `a`b
    case 11:
      r = mxCreateCellArray((mwSize) 1, ndims);
      for(i=0;i<nData;i++)
        mxSetCell(r, i, mxCreateString(kS(x)[i]));
    break;

    //timestamp i.e. dateDtimespan x2
    case 12:
      r = mxCreateDoubleMatrix((mwSize) 1, nData, mxREAL);
      dl = mxGetPr(r);
      for(i=0;i<nData;i++) dl[i] = utmtsp(kJ(x)[i]);
    break;

    //month   i.e. 2010.01 2010.02
    case 13:
      r = mxCreateCellArray((mwSize) 1, ndims);
      for(i=0;i<nData;i++) {
        obj = ka(-KM);
        obj->i = kI(x)[i];
        mxSetCell(r, i, makeatom(obj));
      }
    break;

    //date    i.e. 2010.01.01 2010.01.02
    case 14:
      r = mxCreateDoubleMatrix((mwSize) 1, nData, mxREAL);
      dl = mxGetPr(r);
      for(i=0;i<nData;i++) dl[i] = utmt(kI(x)[i]);
    break;

    //datetime  i.e. 2010.01.01T12:12:12.000 x2
    case 15:
      r = mxCreateDoubleMatrix((mwSize) 1, nData, mxREAL);
      dl = mxGetPr(r);
      for(i=0;i<nData;i++) dl[i] = utmt(kF(x)[i]);
    break;

    //timespan  i.e. 0D00:00:00.000000000 x2
    case 16:
      r = mxCreateDoubleMatrix((mwSize) 1, nData, mxREAL);
      dl = mxGetPr(r);
      for(i=0;i<nData;i++) dl[i] = (double)kJ(x)[i]/8.64e13;
    break;

    //minute  i.e. 00:00 00:01
    case 17:
      r = mxCreateDoubleMatrix((mwSize) 1, nData, mxREAL);
      dl = mxGetPr(r);
      for(i=0;i<nData;i++) dl[i] = (60.0*(double)kI(x)[i])/86400;
    break;

    //second  i.e. 00:00:00 00:00:01
    case 18:
      r = mxCreateDoubleMatrix((mwSize) 1, nData, mxREAL);
      dl = mxGetPr(r);
      for(i=0;i<nData;i++) dl[i] = ((double)kI(x)[i])/86400;
    break;

    //time    i.e. 00:00:00.111 00:00:00.222
    case 19:
      r = mxCreateDoubleMatrix((mwSize) 1, nData, mxREAL);
      dl = mxGetPr(r);
      for(i=0;i<nData;i++) dl[i] = ((double)kI(x)[i]/1000)/86400;
    break;

    default: mexPrintf("list: cannot support %d type\n",x->t); break;
  }

  return r;
}


/**
 * Process table
 */
 mxArray* maketab(K x) {
  mxArray* r = NULL;
  int row,col;
  K flip = ktd(x);
  K colName = kK(flip->k)[0];
  K colData = kK(flip->k)[1];
  int nCols = colName->n;
  int nRows = kK(colData)[0]->n;
  mwSize ndims[] = {(mwSize) 1};
  char** fnames = calloc(nCols, sizeof(char*));

  for (col=0; col<nCols; col++) {
    fnames[col] = (char*)kS(colName)[col];
  }

  // create resulting cell array
  r = mxCreateStructArray((mwSize) 1, ndims, (mwSize) nCols, (const char**)fnames);

  // destroy names
  free(fnames);

  // attach columns
  for (col=0; col<nCols; col++) {
      K list = kK(colData)[col];
      mxSetField(r, 0, kS(colName)[col], makelist(list));
  }

  // unrelease flip
  r0(flip);
  return r;
}

/**
 * Process dictionary
 */
mxArray* makedict(K x) {
  mxArray* r = NULL;
  K keyName = kK(x)[0];
  int nName = keyName->n, row;
  char** fnames;
  mwSize ndims[] = {(mwSize) 1};
  K keyData = kK(x)[1];

  // if it's keyed dict. handle it as a table
  if (keyData->t==98) return maketab(x);

  // allocate memory for keys names
  fnames = calloc(nName, sizeof(char*));

  // collect field names (dicr keys)
  for(row=0;row<nName;row++) {
    fnames[row] = (char*)kS(keyName)[row];
  }

  // create resulting cell array
  r = mxCreateStructArray((mwSize) 1, ndims, (mwSize)nName, (const char**)fnames);

  // simple case like `a`b`c!(1 2 3)
  if(keyData->t > 0) {
    for(row=0;row<nName;row++) {
      if (keyData->t==10) // special case for char list : `a`b`c!("abc")
        mxSetField(r, 0, kS(keyName)[row], makeatom(kValue(keyData->t, ((char*) &kK(keyData)[0])+row)));
      else if (keyData->t==13 || keyData->t==14 || keyData->t==17|| keyData->t==18 || keyData->t==19) // special case for month list : `a`b`c!(2000.01m; 2000.02m; 2000.03m)
        mxSetField(r, 0, kS(keyName)[row], makeatom(kValue(keyData->t, ((int*) &kK(keyData)[0])+row)));
      else // rest cases
        mxSetField(r, 0, kS(keyName)[row], makeatom(kValue(keyData->t, &kK(keyData)[row])));
    }

    free(fnames);
    return r;
  }

  // more complex case
  for(row=0;row<nName;row++){
    if (keyData->t==0 && kK(keyData)[row]->n > 1 && kK(keyData)[row]->t > 0) {
      mxSetField(r, 0, kS(keyName)[row], makelist(kK(keyData)[row]));
    } else {
      mxSetField(r, 0, kS(keyName)[row], makeatom(kK(keyData)[row]));
    }
  }

  free(fnames);
  return r;
}


/**
 * Function entry qdbc
 */
 void mexFunction(int nlhs, mxArray* pout[],
                 int nrhs, const mxArray* pin[]) {
  K res;
  char* query;
  char* host;
  int c, blen, port;
  mxArray* tmp;

  // - check args
  if (nrhs < 2)
    mexErrMsgIdAndTxt(MATID, "Two inputs required.");
  else if(!mxIsStruct(pin[0]))
    mexErrMsgIdAndTxt("qdbc:inputNotStruct", "Input must be a structure.");

  // - get query
  blen = mxGetNumberOfElements(pin[1]) + 1;
  if (blen==1)
    mexErrMsgIdAndTxt("qdbc:emptyQuery", "Query can't be empty");
  query = mxCalloc(blen, sizeof(char));
  mxGetString(pin[1], query, blen);

  // - get host
  tmp = mxGetField(pin[0], 0, "host");
  if (tmp==NULL)
    mexErrMsgIdAndTxt("qdbc:FieldNotSpecified", "Can't find 'host' field in structure");
  blen = mxGetNumberOfElements(tmp) + 1;
  host = mxCalloc(blen, sizeof(char));
  mxGetString(tmp, host, blen);

  // - get port
  tmp = mxGetField(pin[0], 0, "port");
  if (tmp==NULL)
    mexErrMsgIdAndTxt("qdbc:FieldNotSpecified", "Can't find 'port' field in structure");
  port = (int) mxGetScalar(tmp);

  // - connect
  c = khp(host, port);
  if (fcntl(c, F_GETFL) >= 0 && errno != EBADF) {
    // - exec query
    res = k(c, query, (K) 0);
    if (res) {
      mexPrintf("%s>> type %d\n", MATID, res->t);
      if (res->t != -128) {
        if (res->t < 0) pout[0] = makeatom(res);
        else if (res->t >= 0 && res->t <= 19) pout[0] = makelist(res);
        else if (res->t == 98) pout[0] = maketab(res);
        else if (res->t == 99) pout[0] = makedict(res);
        else if (res->t == 101) { pout[0] = mxCreateLogicalScalar(true); mexPrintf("::\n");} // when void query result return t
        else mexPrintf("%s >> Type %d is not supported\n", MATID, res->t);
      } else mexPrintf("%s >> KDB error : %s\n", MATID, res->s);
    } else {
      mexPrintf("%s >> Can't get query result\n", MATID);
    }

    k(c,"",(K)0);
    kclose(c);
  } else {
    mexPrintf("Descriptor is invalid : %d\n", c);
  }
  mxFree(query);
  mxFree(host);
}
