/*
 * $Id$
 *
 * Copyright (C) 2010 Kengo Sato
 *
 * This file is part of IPknot.
 *
 * IPknot is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * IPknot is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with IPknot.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "config.h"
#include "fold.h"

#include <cstring>
#include <iostream>
#include <sstream>

#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "contrafold/SStruct.hpp"
#include "contrafold/InferenceEngine.hpp"
#include "contrafold/Defaults.ipp"

namespace Vienna {
extern "C" {
#include <ViennaRNA/fold.h>
#include <ViennaRNA/fold_vars.h>
#include <ViennaRNA/part_func.h>
#include <ViennaRNA/alifold.h>
#include <ViennaRNA/aln_util.h>
#include <ViennaRNA/utils.h>
#include <ViennaRNA/PS_dot.h>
  extern void read_parameter_file(const char fname[]);

};
};

extern "C"{
#include "ViennaRNA/energy_const.h"
#include "ViennaRNA/energy_par.h"
}

#ifndef FLT_OR_DBL
typedef Vienna::FLT_OR_DBL FLT_OR_DBL;
#endif

#include "nupack/nupack.h"

void copy_boltzmann_parameters();

//extern "C" {
//#include "boltzmann_param.h"
//};

typedef unsigned int uint;

// CONTRAfold model

void
CONTRAfoldModel::
calculate_posterior(const std::string& seq, std::vector<float>& bp, std::vector<int>& offset) const
{
  SStruct ss("unknown", seq);
  ParameterManager<float> pm;
  InferenceEngine<float> en(false);
  std::vector<float> w = GetDefaultComplementaryValues<float>();
  bp.resize((seq.size()+1)*(seq.size()+2)/2, 0.0);
  en.RegisterParameters(pm);
  en.LoadValues(w);
  en.LoadSequence(ss);
  en.ComputeInside();
  en.ComputeOutside();
  en.ComputePosterior();
  en.GetPosterior(0, bp, offset);
}

void
CONTRAfoldModel::
calculate_posterior(const std::string& seq, const std::string& paren,
                    std::vector<float>& bp, std::vector<int>& offset) const
{
  SStruct ss("unknown", seq, paren);
  ParameterManager<float> pm;
  InferenceEngine<float> en(false);
  std::vector<float> w = GetDefaultComplementaryValues<float>();
  bp.resize((seq.size()+1)*(seq.size()+2)/2, 0.0);
  en.RegisterParameters(pm);
  en.LoadValues(w);
  en.LoadSequence(ss);
  en.UseConstraints(ss.GetMapping());
  en.ComputeInside();
  en.ComputeOutside();
  en.ComputePosterior();
  en.GetPosterior(0, bp, offset);
}

// RNAfold model

RNAfoldModel::
RNAfoldModel(const char* param)
{
  if (param)
  {
    if (strcmp(param, "default")!=0)
      Vienna::read_parameter_file(param);
  }
  else
    copy_boltzmann_parameters();
}

void
RNAfoldModel::
calculate_posterior(const std::string& seq, const std::string& paren,
                    std::vector<float>& bp, std::vector<int>& offset) const
{
  std::string p(paren);
  std::replace(p.begin(), p.end(), '.', 'x');
  std::replace(p.begin(), p.end(), '?', '.');

  int bk = Vienna::fold_constrained;
  Vienna::fold_constrained = 1;

  uint L=seq.size();
  bp.resize((L+1)*(L+2)/2);
  offset.resize(L+1);
  for (uint i=0; i<=L; ++i)
    offset[i] = i*((L+1)+(L+1)-i-1)/2;
#if 0
  std::string str(seq.size()+1, '.');
  float min_en = Vienna::fold(const_cast<char*>(seq.c_str()), &str[0]);
  float sfact = 1.07;
  float kT = (Vienna::temperature+273.15)*1.98717/1000.; /* in Kcal */
  Vienna::pf_scale = exp(-(sfact*min_en)/kT/seq.size());
#else
  Vienna::pf_scale = -1;
#endif
#ifndef HAVE_VIENNA20
  Vienna::init_pf_fold(L);
#endif
  Vienna::pf_fold(const_cast<char*>(seq.c_str()), &p[0]);
#ifdef HAVE_VIENNA20
  FLT_OR_DBL* pr = Vienna::export_bppm();
  int* iindx = Vienna::get_iindx(seq.size());
#else
  FLT_OR_DBL* pr = Vienna::pr;
  int* iindx = Vienna::iindx;
#endif
  for (uint i=0; i!=L-1; ++i)
    for (uint j=i+1; j!=L; ++j)
      bp[offset[i+1]+(j+1)] = pr[iindx[i+1]-(j+1)];
  Vienna::free_pf_arrays();
  Vienna::fold_constrained = bk;
}

void
RNAfoldModel::
calculate_posterior(const std::string& seq, std::vector<float>& bp, std::vector<int>& offset) const
{
  uint L=seq.size();
  bp.resize((L+1)*(L+2)/2);
  offset.resize(L+1);
  for (uint i=0; i<=L; ++i)
    offset[i] = i*((L+1)+(L+1)-i-1)/2;
#if 0
  std::string str(seq.size()+1, '.');
  float min_en = Vienna::fold(const_cast<char*>(seq.c_str()), &str[0]);
  float sfact = 1.07;
  float kT = (Vienna::temperature+273.15)*1.98717/1000.; /* in Kcal */
  Vienna::pf_scale = exp(-(sfact*min_en)/kT/seq.size());
#else
  Vienna::pf_scale = -1;
#endif
#ifndef HAVE_VIENNA20
  Vienna::init_pf_fold(L);
#endif
  Vienna::pf_fold(const_cast<char*>(seq.c_str()), NULL);
#ifdef HAVE_VIENNA20
  FLT_OR_DBL* pr = Vienna::export_bppm();
  int* iindx = Vienna::get_iindx(seq.size());
#else
  FLT_OR_DBL* pr = Vienna::pr;
  int* iindx = Vienna::iindx;
#endif
  for (uint i=0; i!=L-1; ++i)
    for (uint j=i+1; j!=L; ++j)
      bp[offset[i+1]+(j+1)] = pr[iindx[i+1]-(j+1)];
  Vienna::free_pf_arrays();
}

// Nupack model

void
NupackModel::
calculate_posterior(const std::string& seq, std::vector<float>& bp, std::vector<int>& offset) const
{
  Nupack<long double> nu;
  if (param_)
    nu.load_parameters(param_);
  else
    nu.load_default_parameters(/*model_*/);
  //nu.dump_parameters(std::cout);
  nu.load_sequence(seq);
  nu.calculate_partition_function();
  nu.calculate_posterior();
  nu.get_posterior(bp, offset);
}


// Alifold model

AlifoldModel::
AlifoldModel(const char* param)
{
  if (param)
  {
    if (strcmp(param, "default")!=0)
      Vienna::read_parameter_file(param);
  }
  else
    copy_boltzmann_parameters();
}

static char** alloc_aln(const std::list<std::string>& aln)
{
  uint L=aln.front().size();
  char** seqs = new char*[aln.size()+1];
  seqs[aln.size()]=NULL;
  std::list<std::string>::const_iterator x;
  uint i=0;
  for (x=aln.begin(); x!=aln.end(); ++x)
  {
    seqs[i] = new char[L+1];
    strcpy(seqs[i++], x->c_str());
  }
  return seqs;
}

static void free_aln(char** seqs)
{
  for (uint i=0; seqs[i]!=NULL; ++i) delete[] seqs[i];
  delete[] seqs;
}

void
AlifoldModel::
calculate_posterior(const std::list<std::string>& aln, const std::string& paren,
                    std::vector<float>& bp, std::vector<int>& offset) const
{
  std::string p(paren);
  std::replace(p.begin(), p.end(), '.', 'x');
  std::replace(p.begin(), p.end(), '?', '.');

  int bk = Vienna::fold_constrained;
  Vienna::fold_constrained = 1;

  //uint N=aln.size();
  uint L=aln.front().size();
  bp.resize((L+1)*(L+2)/2, 0.0);
  offset.resize(L+1);
  for (uint i=0; i<=L; ++i)
    offset[i] = i*((L+1)+(L+1)-i-1)/2;

  char** seqs=alloc_aln(aln);
  std::string res(p);
  // scaling parameters to avoid overflow
#ifdef HAVE_VIENNA20
  double min_en = Vienna::alifold((const char**)seqs, &res[0]);
#else
  double min_en = Vienna::alifold(seqs, &res[0]);
#endif
  double kT = (Vienna::temperature+273.15)*1.98717/1000.; /* in Kcal */
  Vienna::pf_scale = exp(-(1.07*min_en)/kT/L);
  Vienna::free_alifold_arrays();

#ifdef HAVE_VIENNA18
  Vienna::plist* pi;
#else
  Vienna::pair_info* pi;
#endif
#ifdef HAVE_VIENNA20
  Vienna::alipf_fold((const char**)seqs, &p[0], &pi);
#else
  Vienna::alipf_fold(seqs, &p[0], &pi);
#endif
  for (uint k=0; pi[k].i!=0; ++k)
    bp[offset[pi[k].i]+pi[k].j]=pi[k].p;
  free(pi);

  Vienna::free_alipf_arrays();
  free_aln(seqs);
  Vienna::fold_constrained = bk;
}

void
AlifoldModel::
calculate_posterior(const std::list<std::string>& aln,
                    std::vector<float>& bp, std::vector<int>& offset) const
{

  //uint N=aln.size();
  uint L=aln.front().size();
  bp.resize((L+1)*(L+2)/2, 0.0);
  offset.resize(L+1);
  for (uint i=0; i<=L; ++i)
    offset[i] = i*((L+1)+(L+1)-i-1)/2;

  char** seqs=alloc_aln(aln);
  std::string res(L+1, ' ');
  // scaling parameters to avoid overflow
#ifdef HAVE_VIENNA20
  double min_en = Vienna::alifold((const char**)seqs, &res[0]);
#else
  double min_en = Vienna::alifold(seqs, &res[0]);
#endif
  double kT = (Vienna::temperature+273.15)*1.98717/1000.; /* in Kcal */
  Vienna::pf_scale = exp(-(1.07*min_en)/kT/L);
  Vienna::free_alifold_arrays();

#ifdef HAVE_VIENNA18
  Vienna::plist* pi;
#else
  Vienna::pair_info* pi;
#endif
#ifdef HAVE_VIENNA20
  Vienna::alipf_fold((const char**)seqs, NULL, &pi);
#else
  Vienna::alipf_fold(seqs, NULL, &pi);
#endif
  for (uint k=0; pi[k].i!=0; ++k)
    bp[offset[pi[k].i]+pi[k].j]=pi[k].p;
  free(pi);

  Vienna::free_alipf_arrays();
  free_aln(seqs);
}

// Averaged model
void
AveragedModel::
calculate_posterior(const std::list<std::string>& aln,
                    std::vector<float>& bp, std::vector<int>& offset) const
{
  uint N=aln.size();
  uint L=aln.front().size();
  bp.resize((L+1)*(L+2)/2, 0.0);
  offset.resize(L+1);
  for (uint i=0; i<=L; ++i)
    offset[i] = i*((L+1)+(L+1)-i-1)/2;
  for (std::list<std::string>::const_iterator s=aln.begin(); s!=aln.end(); ++s)
  {
    std::vector<float> lbp;
    std::vector<int> loffset;
    std::string seq;
    std::vector<int> idx;
    for (uint i=0; i!=s->size(); ++i)
    {
      if ((*s)[i]!='-')
      {
        seq.push_back((*s)[i]);
        idx.push_back(i);
      }
    }
    en_->calculate_posterior(seq, lbp, loffset);
    for (uint i=0; i!=seq.size()-1; ++i)
      for (uint j=i+1; j!=seq.size(); ++j)
        bp[offset[idx[i]+1]+(idx[j]+1)] += lbp[loffset[i+1]+(j+1)]/N;
  }
}

static
std::vector<int>
bpseq(const std::string& paren)
{
  std::vector<int> ret(paren.size(), -1);
  std::stack<int> st;
  for (uint i=0; i!=paren.size(); ++i)
  {
    if (paren[i]=='(')
    {
      st.push(i);
    }
    else if (paren[i]==')')
    {
      ret[st.top()]=i;
      ret[i]=st.top();
      st.pop();
    }
  }
  return ret;
}

void
AveragedModel::
calculate_posterior(const std::list<std::string>& aln, const std::string& paren,
                    std::vector<float>& bp, std::vector<int>& offset) const
{
  uint N=aln.size();
  uint L=aln.front().size();
  bp.resize((L+1)*(L+2)/2, 0.0);
  offset.resize(L+1);
  for (uint i=0; i<=L; ++i)
    offset[i] = i*((L+1)+(L+1)-i-1)/2;
  std::vector<int> p = bpseq(paren);
  for (std::list<std::string>::const_iterator s=aln.begin(); s!=aln.end(); ++s)
  {
    std::vector<float> lbp;
    std::vector<int> loffset;
    std::string seq;
    std::vector<int> idx;
    std::vector<int> rev(s->size(), -1);
    for (uint i=0; i!=s->size(); ++i)
    {
      if ((*s)[i]!='-')
      {
        seq.push_back((*s)[i]);
        idx.push_back(i);
        rev[i]=seq.size()-1;
      }
    }
    std::string lparen(seq.size(), '.');
    for (uint i=0; i!=p.size(); ++i)
    {
      if (rev[i]>=0)
      {
        if (p[i]<0 || rev[p[i]]>=0)
          lparen[rev[i]] = paren[i];
        else
          lparen[rev[i]] = '.';
      }
    }

    en_->calculate_posterior(seq, lparen, lbp, loffset);
    for (uint i=0; i!=seq.size()-1; ++i)
      for (uint j=i+1; j!=seq.size(); ++j)
        bp[offset[idx[i]+1]+(idx[j]+1)] += lbp[loffset[i+1]+(j+1)]/N;
  }
}

// MixtureModel
void
MixtureModel::
calculate_posterior(const std::list<std::string>& aln,
                    std::vector<float>& bp, std::vector<int>& offset) const
{
  uint L=aln.front().size();
  bp.resize((L+1)*(L+2)/2);
  std::fill(bp.begin(), bp.end(), 0.0);
  offset.resize(L+1);
  for (uint i=0; i<=L; ++i)
    offset[i] = i*((L+1)+(L+1)-i-1)/2;
  assert(en_.size()==w_.size());
  for (uint i=0; i!=en_.size(); ++i)
  {
    std::vector<float> lbp;
    std::vector<int> loffset;
    en_[i]->calculate_posterior(aln, lbp, loffset);
    for (uint j=0; j!=bp.size(); ++j)
      bp[j] += lbp[j]*w_[i];
  }
}

void
MixtureModel::
calculate_posterior(const std::list<std::string>& aln, const std::string& paren,
                    std::vector<float>& bp, std::vector<int>& offset) const
{
  uint L=aln.front().size();
  bp.resize((L+1)*(L+2)/2);
  std::fill(bp.begin(), bp.end(), 0.0);
  offset.resize(L+1);
  for (uint i=0; i<=L; ++i)
    offset[i] = i*((L+1)+(L+1)-i-1)/2;
  assert(en_.size()==w_.size());
  for (uint i=0; i!=en_.size(); ++i)
  {
    std::vector<float> lbp;
    std::vector<int> loffset;
    en_[i]->calculate_posterior(aln, paren, lbp, loffset);
    for (uint j=0; j!=bp.size(); ++j)
      bp[j] += lbp[j]*w_[i];
  }
}

// Aux model
bool
AuxModel::
calculate_posterior(const char* filename, std::string& seq,
                    std::vector<float>& bp, std::vector<int>& offset) const
{
  std::string l;
  uint L=0;
  std::ifstream in(filename);
  if (!in) return false;
  while (std::getline(in, l)) ++L;
  bp.resize((L+1)*(L+2)/2, 0.0);
  offset.resize(L+1);
  for (uint i=0; i<=L; ++i)
    offset[i] = i*((L+1)+(L+1)-i-1)/2;
  seq.resize(L);
  in.clear();
  in.seekg(0, std::ios::beg);
  while (std::getline(in, l))
  {
    std::vector<std::string> v;
    std::istringstream ss(l);
    std::string s;
    while (ss >> s) v.push_back(s);
    uint up = atoi(v[0].c_str());
    seq[up-1] = v[1][0];
    for (uint i=2; i!=v.size(); ++i)
    {
      uint down;
      float p;
      if (sscanf(v[i].c_str(), "%u:%f", &down, &p)==2)
        bp[offset[up]+down]=p;
    }
  }
  return true;
}

// BOLTZMANN PARAMS

#define DEF -50
#define NST 0

/* stack_energies */
static int stack37a[] = {
/*  CG    GC    GU    UG    AU    UA    @  */
   -133, -207, -146,  -37, -139, -132, NST,
   -207, -205, -150,  -91, -129, -123, NST,
   -146, -150,  -22,  -67,  -81,  -58, NST,
    -37,  -91,  -67,  -38,  -14,   -2, NST,
   -139, -129,  -81,  -14,  -84,  -69, NST,
   -132, -123,  -58,   -2,  -69,  -68, NST,
    NST,  NST,  NST,  NST,  NST,  NST, NST
};

/* mismatch_hairpin */
static int mismatchH37a[] = {
     0,    0,    0,    0,    0,
   -90,  -35,  -13,    3,   13,
   -90,  -12,  -17,  -98,  -22,
   -90,  -51,  -24,   -1,    3,
   -90,  -16,  -57,  -97,  -38,
     0,    0,    0,    0,    0,
   -70,    1,  -22,  -65,  110,
   -70,   11,  -27,  -17,    0,
   -70,  -67,    1,  -45,   36,
   -70,    8,  -39,  -55,  -92,
     0,    0,    0,    0,    0,
     0,   60,   75,   43,  180,
     0,   50,   44,   70,   -1,
     0,  -92,   15,   39,  114,
     0,   25,    7,   67,   36,
     0,    0,    0,    0,    0,
     0,   43,   52,   87,  109,
     0,   52,   35,  -10,   40,
     0,  -26,  -28,    5,   92,
     0,   42,  -38,  -14,  -30,
     0,    0,    0,    0,    0,
     0,   41,   77,   65,  117,
     0,   -2,    8,   44,   43,
     0,  -31,   23,   15,   60,
     0,   29,   -6,   32,  -10,
     0,    0,    0,    0,    0,
     0,   17,   52,   76,   68,
     0,   53,   36,   37,   11,
     0,  -39,   46,   16,   68,
     0,   50,  -16,   46,   29,
     0,    0,    0,    0,    0,
     0,    0,    0,    0,    0,
     0,    0,    0,    0,    0,
     0,    0,    0,    0,    0,
     0,    0,    0,    0,    0
};

/* mismatch_interior */
static int mismatchI37a[] = {
     0,    0,    0,    0,    0,
     0,    0,    0,  -50,    0,
     0,    0,    0,    0,    0,
     0,  -50,    0,    0,    0,
     0,    0,    0,    0,  -45,
     0,    0,    0,    0,    0,
     0,    0,    0,  -50,    0,
     0,    0,    0,    0,    0,
     0,  -50,    0,    0,    0,
     0,    0,    0,    0,  -45,
     0,    0,    0,    0,    0,
     0,   63,   63,   13,   63,
     0,   63,   63,   63,   63,
     0,   13,   63,   63,   63,
     0,   63,   63,   63,   18,
     0,    0,    0,    0,    0,
     0,   63,   63,   13,   63,
     0,   63,   63,   63,   63,
     0,   13,   63,   63,   63,
     0,   63,   63,   63,   18,
     0,    0,    0,    0,    0,
     0,   63,   63,   13,   63,
     0,   63,   63,   63,   63,
     0,   13,   63,   63,   63,
     0,   63,   63,   63,   18,
     0,    0,    0,    0,    0,
     0,   63,   63,   13,   63,
     0,   63,   63,   63,   63,
     0,   13,   63,   63,   63,
     0,   63,   63,   63,   18,
    90,   90,   90,   90,   90,
    90,   90,   90,   90,  -20,
    90,   90,   90,   90,   90,
    90,  -20,   90,   90,   90,
    90,   90,   90,   90,   20
};

/* dangle5 */
static int dangle5_37a[] = {
/*  @     A     C     G     U   */
   INF,  INF,  INF,  INF,  INF,
   INF,   -8,    0,    0,    0,
   INF,  -10,    0,    0,    0,
   INF,    0,    0,    0,    0,
   INF,    0,    0,    0,    0,
   INF,  -10,    0,    0,    0,
   INF,   -3,    0,    0,    0,
     0,    0,    0,    0,    0
};

/* dangle3 */
static int dangle3_37a[] = {
/*  @     A     C     G     U   */
   INF,  INF,  INF,  INF,  INF,
   INF,  -10,   -9,  -51,  -14,
   INF,  -41,    0,  -46,  -35,
   INF,  -10,    0,  -50,   -2,
   INF,  -10,  -40,  -60,    0,
   INF,  -10,  -13,  -25,   -7,
   INF,  -10,  -30,  -44,   -7,
     0,    0,    0,    0,    0
};

/* int11_energies */
static int int11_37a[] = {
/* CG..CG */
   158,  158,  158,  158,  158,
   158,  158,   74,   97,   35,
   158,   74,   89,   35,   78,
   158,   97,   35,   58,   35,
   158,   35,   78,   35,  -12,
/* CG..GC */
   158,  158,  158,  158,  158,
   158,   76,   18,    6,   35,
   158,   46,   85,   35,  124,
   158,   72,   35,  -27,   35,
   158,   35,   51,   35,  -20,
/* CG..GU */
   158,  158,  158,  158,  158,
   158,   98,   98,   98,   98,
   158,   98,   98,   98,   98,
   158,   98,   98,   43,   98,
   158,   98,   98,   98,   98,
/* CG..UG */
   158,  158,  158,  158,  158,
   158,   98,   98,   98,   98,
   158,   98,   98,   98,   98,
   158,   98,   98,   43,   98,
   158,   98,   98,   98,   98,
/* CG..AU */
   158,  158,  158,  158,  158,
   158,   98,   98,   98,   98,
   158,   98,   98,   98,   98,
   158,   98,   98,   43,   98,
   158,   98,   98,   98,   53,
/* CG..UA */
   158,  158,  158,  158,  158,
   158,   98,   98,   98,   98,
   158,   98,   98,   98,   98,
   158,   98,   98,   43,   98,
   158,   98,   98,   98,   36,
/* CG.. @ */
   158,  158,  158,  158,  158,
   158,  158,  158,  158,  158,
   158,  158,  158,  158,  158,
   158,  158,  158,  158,  158,
   158,  158,  158,  158,  158,
/* GC..CG */
   158,  158,  158,  158,  158,
   158,   76,   46,   72,   35,
   158,   18,   85,   35,   51,
   158,    6,   35,  -27,   35,
   158,   35,  124,   35,  -20,
/* GC..GC */
   158,  158,  158,  158,  158,
   158,  114,   11,   38,   35,
   158,   11,   43,   35,  -10,
   158,   38,   35,  -51,   35,
   158,   35,  -10,   35, -161,
/* GC..GU */
   158,  158,  158,  158,  158,
   158,   98,   98,   98,   98,
   158,   98,   98,   98,   98,
   158,   98,   98,   43,   98,
   158,   98,   98,   98,   98,
/* GC..UG */
   158,  158,  158,  158,  158,
   158,   98,   98,   98,   98,
   158,   98,   98,   98,   98,
   158,   98,   98,   43,   98,
   158,   98,   98,   98,   98,
/* GC..AU */
   158,  158,  158,  158,  158,
   158,   98,   98,   98,   98,
   158,   98,   98,   98,   98,
   158,   98,   98,   43,   98,
   158,   98,   98,   98,  -18,
/* GC..UA */
   158,  158,  158,  158,  158,
   158,   98,   98,   98,   98,
   158,   98,   98,   98,   98,
   158,   98,   98,   43,   98,
   158,   98,   98,   98,   68,
/* GC.. @ */
   158,  158,  158,  158,  158,
   158,  158,  158,  158,  158,
   158,  158,  158,  158,  158,
   158,  158,  158,  158,  158,
   158,  158,  158,  158,  158,
/* GU..CG */
   158,  158,  158,  158,  158,
   158,   98,   98,   98,   98,
   158,   98,   98,   98,   98,
   158,   98,   98,   43,   98,
   158,   98,   98,   98,   98,
/* GU..GC */
   158,  158,  158,  158,  158,
   158,   98,   98,   98,   98,
   158,   98,   98,   98,   98,
   158,   98,   98,   43,   98,
   158,   98,   98,   98,   98,
/* GU..GU */
   161,  161,  161,  161,  161,
   161,  161,  161,  161,  161,
   161,  161,  161,  161,  161,
   161,  161,  161,  106,  161,
   161,  161,  161,  161,  161,
/* GU..UG */
   161,  161,  161,  161,  161,
   161,  161,  161,  161,  161,
   161,  161,  161,  161,  161,
   161,  161,  161,  106,  161,
   161,  161,  161,  161,  161,
/* GU..AU */
   161,  161,  161,  161,  161,
   161,  161,  161,  161,  161,
   161,  161,  161,  161,  161,
   161,  161,  161,  106,  161,
   161,  161,  161,  161,  161,
/* GU..UA */
   161,  161,  161,  161,  161,
   161,  161,  161,  161,  161,
   161,  161,  161,  161,  161,
   161,  161,  161,  106,  161,
   161,  161,  161,  161,  161,
/* GU.. @ */
   161,  161,  161,  161,  161,
   161,  161,  161,  161,  161,
   161,  161,  161,  161,  161,
   161,  161,  161,  161,  161,
   161,  161,  161,  161,  161,
/* UG..CG */
   158,  158,  158,  158,  158,
   158,   98,   98,   98,   98,
   158,   98,   98,   98,   98,
   158,   98,   98,   43,   98,
   158,   98,   98,   98,   98,
/* UG..GC */
   158,  158,  158,  158,  158,
   158,   98,   98,   98,   98,
   158,   98,   98,   98,   98,
   158,   98,   98,   43,   98,
   158,   98,   98,   98,   98,
/* UG..GU */
   161,  161,  161,  161,  161,
   161,  161,  161,  161,  161,
   161,  161,  161,  161,  161,
   161,  161,  161,  106,  161,
   161,  161,  161,  161,  161,
/* UG..UG */
   161,  161,  161,  161,  161,
   161,  161,  161,  161,  161,
   161,  161,  161,  161,  161,
   161,  161,  161,  106,  161,
   161,  161,  161,  161,  161,
/* UG..AU */
   161,  161,  161,  161,  161,
   161,  161,  161,  161,  161,
   161,  161,  161,  161,  161,
   161,  161,  161,  106,  161,
   161,  161,  161,  161,  161,
/* UG..UA */
   161,  161,  161,  161,  161,
   161,  161,  161,  161,  161,
   161,  161,  161,  161,  161,
   161,  161,  161,  106,  161,
   161,  161,  161,  161,  161,
/* UG.. @ */
   161,  161,  161,  161,  161,
   161,  161,  161,  161,  161,
   161,  161,  161,  161,  161,
   161,  161,  161,  161,  161,
   161,  161,  161,  161,  161,
/* AU..CG */
   158,  158,  158,  158,  158,
   158,   98,   98,   98,   98,
   158,   98,   98,   98,   98,
   158,   98,   98,   43,   98,
   158,   98,   98,   98,   53,
/* AU..GC */
   158,  158,  158,  158,  158,
   158,   98,   98,   98,   98,
   158,   98,   98,   98,   98,
   158,   98,   98,   43,   98,
   158,   98,   98,   98,  -18,
/* AU..GU */
   161,  161,  161,  161,  161,
   161,  161,  161,  161,  161,
   161,  161,  161,  161,  161,
   161,  161,  161,  106,  161,
   161,  161,  161,  161,  161,
/* AU..UG */
   161,  161,  161,  161,  161,
   161,  161,  161,  161,  161,
   161,  161,  161,  161,  161,
   161,  161,  161,  106,  161,
   161,  161,  161,  161,  161,
/* AU..AU */
   161,  161,  161,  161,  161,
   161,  161,  161,  161,  161,
   161,  161,  161,  161,  161,
   161,  161,  161,  106,  161,
   161,  161,  161,  161,   45,
/* AU..UA */
   161,  161,  161,  161,  161,
   161,  161,  161,  161,  161,
   161,  161,  161,  161,  161,
   161,  161,  161,  106,  161,
   161,  161,  161,  161,   62,
/* AU.. @ */
   161,  161,  161,  161,  161,
   161,  161,  161,  161,  161,
   161,  161,  161,  161,  161,
   161,  161,  161,  161,  161,
   161,  161,  161,  161,  161,
/* UA..CG */
   158,  158,  158,  158,  158,
   158,   98,   98,   98,   98,
   158,   98,   98,   98,   98,
   158,   98,   98,   43,   98,
   158,   98,   98,   98,   36,
/* UA..GC */
   158,  158,  158,  158,  158,
   158,   98,   98,   98,   98,
   158,   98,   98,   98,   98,
   158,   98,   98,   43,   98,
   158,   98,   98,   98,   68,
/* UA..GU */
   161,  161,  161,  161,  161,
   161,  161,  161,  161,  161,
   161,  161,  161,  161,  161,
   161,  161,  161,  106,  161,
   161,  161,  161,  161,  161,
/* UA..UG */
   161,  161,  161,  161,  161,
   161,  161,  161,  161,  161,
   161,  161,  161,  161,  161,
   161,  161,  161,  106,  161,
   161,  161,  161,  161,  161,
/* UA..AU */
   161,  161,  161,  161,  161,
   161,  161,  161,  161,  161,
   161,  161,  161,  161,  161,
   161,  161,  161,  106,  161,
   161,  161,  161,  161,   62,
/* UA..UA */
   161,  161,  161,  161,  161,
   161,  161,  161,  161,  161,
   161,  161,  161,  161,  161,
   161,  161,  161,  106,  161,
   161,  161,  161,  161,  124,
/* UA.. @ */
   161,  161,  161,  161,  161,
   161,  161,  161,  161,  161,
   161,  161,  161,  161,  161,
   161,  161,  161,  161,  161,
   161,  161,  161,  161,  161,
/*  @..CG */
   158,  158,  158,  158,  158,
   158,  158,  158,  158,  158,
   158,  158,  158,  158,  158,
   158,  158,  158,  158,  158,
   158,  158,  158,  158,  158,
/*  @..GC */
   158,  158,  158,  158,  158,
   158,  158,  158,  158,  158,
   158,  158,  158,  158,  158,
   158,  158,  158,  158,  158,
   158,  158,  158,  158,  158,
/*  @..GU */
   161,  161,  161,  161,  161,
   161,  161,  161,  161,  161,
   161,  161,  161,  161,  161,
   161,  161,  161,  161,  161,
   161,  161,  161,  161,  161,
/*  @..UG */
   161,  161,  161,  161,  161,
   161,  161,  161,  161,  161,
   161,  161,  161,  161,  161,
   161,  161,  161,  161,  161,
   161,  161,  161,  161,  161,
/*  @..AU */
   161,  161,  161,  161,  161,
   161,  161,  161,  161,  161,
   161,  161,  161,  161,  161,
   161,  161,  161,  161,  161,
   161,  161,  161,  161,  161,
/*  @..UA */
   161,  161,  161,  161,  161,
   161,  161,  161,  161,  161,
   161,  161,  161,  161,  161,
   161,  161,  161,  161,  161,
   161,  161,  161,  161,  161,
/*  @.. @ */
   161,  161,  161,  161,  161,
   161,  161,  161,  161,  161,
   161,  161,  161,  161,  161,
   161,  161,  161,  161,  161,
   161,  161,  161,  161,  161
};

/* int21_energies */
static int int21_37a[] = {
/* CG.@..CG */
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
/* CG.A..CG */
   324,  324,  324,  324,  324,
   324,  154,  144,  127,  171,
   324,  174,  171,  185,  171,
   324,  142,  143,  107,  171,
   324,  171,  171,  171,  171,
/* CG.C..CG */
   324,  324,  324,  324,  324,
   324,   83,  166,  171,  180,
   324,  161,  159,  171,  170,
   324,  171,  171,  171,  171,
   324,  128,  136,  171,  166,
/* CG.G..CG */
   324,  324,  324,  324,  324,
   324,   98,  171,   83,  171,
   324,  171,  171,  171,  171,
   324,  132,  171,  104,  171,
   324,  171,  171,  171,  171,
/* CG.U..CG */
   324,  324,  324,  324,  324,
   324,  171,  171,  171,  171,
   324,  171,  164,  171,  190,
   324,  171,  171,  171,  171,
   324,  171,  128,  171,  119,
/* CG.@..GC */
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
/* CG.A..GC */
   324,  324,  324,  324,  324,
   324,  172,  145,    4,  171,
   324,  193,  157,  166,  171,
   324,  166,  132,  121,  171,
   324,  171,  171,  171,  171,
/* CG.C..GC */
   324,  324,  324,  324,  324,
   324,  122,  149,  171,  166,
   324,  152,  155,  171,  183,
   324,  171,  171,  171,  171,
   324,  171,  147,  171,  176,
/* CG.G..GC */
   324,  324,  324,  324,  324,
   324,  143,  171,   77,  171,
   324,  171,  171,  171,  171,
   324,  193,  171,  230,  171,
   324,  171,  171,  171,  171,
/* CG.U..GC */
   324,  324,  324,  324,  324,
   324,  171,  171,  171,  171,
   324,  171,  158,  171,  149,
   324,  171,  171,  171,  171,
   324,  171,  107,  171,   76,
/* CG.@..GU */
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
/* CG.A..GU */
   324,  324,  324,  324,  324,
   324,  221,  211,  194,  238,
   324,  241,  238,  252,  238,
   324,  209,  210,  174,  238,
   324,  238,  238,  238,  238,
/* CG.C..GU */
   324,  324,  324,  324,  324,
   324,  150,  233,  238,  247,
   324,  228,  226,  238,  237,
   324,  238,  238,  238,  238,
   324,  195,  203,  238,  233,
/* CG.G..GU */
   324,  324,  324,  324,  324,
   324,  165,  238,  150,  238,
   324,  238,  238,  238,  238,
   324,  199,  238,  171,  238,
   324,  238,  238,  238,  238,
/* CG.U..GU */
   324,  324,  324,  324,  324,
   324,  238,  238,  238,  238,
   324,  238,  231,  238,  257,
   324,  238,  238,  238,  238,
   324,  238,  195,  238,  186,
/* CG.@..UG */
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
/* CG.A..UG */
   324,  324,  324,  324,  324,
   324,  221,  211,  194,  238,
   324,  241,  238,  252,  238,
   324,  209,  210,  174,  238,
   324,  238,  238,  238,  238,
/* CG.C..UG */
   324,  324,  324,  324,  324,
   324,  150,  233,  238,  247,
   324,  228,  226,  238,  237,
   324,  238,  238,  238,  238,
   324,  195,  203,  238,  233,
/* CG.G..UG */
   324,  324,  324,  324,  324,
   324,  165,  238,  150,  238,
   324,  238,  238,  238,  238,
   324,  199,  238,  171,  238,
   324,  238,  238,  238,  238,
/* CG.U..UG */
   324,  324,  324,  324,  324,
   324,  238,  238,  238,  238,
   324,  238,  231,  238,  257,
   324,  238,  238,  238,  238,
   324,  238,  195,  238,  186,
/* CG.@..AU */
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
/* CG.A..AU */
   324,  324,  324,  324,  324,
   324,  221,  211,  194,  238,
   324,  241,  238,  252,  238,
   324,  209,  210,  174,  238,
   324,  238,  238,  238,  238,
/* CG.C..AU */
   324,  324,  324,  324,  324,
   324,  150,  233,  238,  247,
   324,  228,  226,  238,  237,
   324,  238,  238,  238,  238,
   324,  195,  203,  238,  233,
/* CG.G..AU */
   324,  324,  324,  324,  324,
   324,  165,  238,  150,  238,
   324,  238,  238,  238,  238,
   324,  199,  238,  171,  238,
   324,  238,  238,  238,  238,
/* CG.U..AU */
   324,  324,  324,  324,  324,
   324,  238,  238,  238,  238,
   324,  238,  231,  238,  257,
   324,  238,  238,  238,  238,
   324,  238,  195,  238,  186,
/* CG.@..UA */
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
/* CG.A..UA */
   324,  324,  324,  324,  324,
   324,  221,  211,  194,  238,
   324,  241,  238,  252,  238,
   324,  209,  210,  174,  238,
   324,  238,  238,  238,  238,
/* CG.C..UA */
   324,  324,  324,  324,  324,
   324,  150,  233,  238,  247,
   324,  228,  226,  238,  237,
   324,  238,  238,  238,  238,
   324,  195,  203,  238,  233,
/* CG.G..UA */
   324,  324,  324,  324,  324,
   324,  165,  238,  150,  238,
   324,  238,  238,  238,  238,
   324,  199,  238,  171,  238,
   324,  238,  238,  238,  238,
/* CG.U..UA */
   324,  324,  324,  324,  324,
   324,  238,  238,  238,  238,
   324,  238,  231,  238,  257,
   324,  238,  238,  238,  238,
   324,  238,  195,  238,  186,
/* CG.@.. @ */
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
/* CG.A.. @ */
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
/* CG.C.. @ */
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
/* CG.G.. @ */
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
/* CG.U.. @ */
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
/* GC.@..CG */
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
/* GC.A..CG */
   324,  324,  324,  324,  324,
   324,  137,  144,  250,  171,
   324,  157,  186,  205,  171,
   324,  119,  154,   95,  171,
   324,  171,  171,  171,  171,
/* GC.C..CG */
   324,  324,  324,  324,  324,
   324,   45,  184,  171,  195,
   324,  171,  164,  171,  159,
   324,  171,  171,  171,  171,
   324,   86,  127,  171,  157,
/* GC.G..CG */
   324,  324,  324,  324,  324,
   324,   55,  171,   91,  171,
   324,  171,  171,  171,  171,
   324,   72,  171,  -23,  171,
   324,  171,  171,  171,  171,
/* GC.U..CG */
   324,  324,  324,  324,  324,
   324,  171,  171,  171,  171,
   324,  171,  170,  171,  232,
   324,  171,  171,  171,  171,
   324,  171,  150,  171,  162,
/* GC.@..GC */
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
/* GC.A..GC */
   324,  324,  324,  324,  324,
   324,  154,  144,  127,  171,
   324,  174,  171,  185,  171,
   324,  142,  143,  107,  171,
   324,  171,  171,  171,  171,
/* GC.C..GC */
   324,  324,  324,  324,  324,
   324,   83,  166,  171,  180,
   324,  161,  159,  171,  170,
   324,  171,  171,  171,  171,
   324,  128,  136,  171,  166,
/* GC.G..GC */
   324,  324,  324,  324,  324,
   324,   98,  171,   83,  171,
   324,  171,  171,  171,  171,
   324,  132,  171,  104,  171,
   324,  171,  171,  171,  171,
/* GC.U..GC */
   324,  324,  324,  324,  324,
   324,  171,  171,  171,  171,
   324,  171,  164,  171,  190,
   324,  171,  171,  171,  171,
   324,  171,  128,  171,  119,
/* GC.@..GU */
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
/* GC.A..GU */
   324,  324,  324,  324,  324,
   324,  221,  211,  194,  238,
   324,  241,  238,  252,  238,
   324,  209,  210,  174,  238,
   324,  238,  238,  238,  238,
/* GC.C..GU */
   324,  324,  324,  324,  324,
   324,  150,  233,  238,  247,
   324,  228,  226,  238,  237,
   324,  238,  238,  238,  238,
   324,  195,  203,  238,  233,
/* GC.G..GU */
   324,  324,  324,  324,  324,
   324,  165,  238,  150,  238,
   324,  238,  238,  238,  238,
   324,  199,  238,  171,  238,
   324,  238,  238,  238,  238,
/* GC.U..GU */
   324,  324,  324,  324,  324,
   324,  238,  238,  238,  238,
   324,  238,  231,  238,  257,
   324,  238,  238,  238,  238,
   324,  238,  195,  238,  186,
/* GC.@..UG */
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
/* GC.A..UG */
   324,  324,  324,  324,  324,
   324,  221,  211,  194,  238,
   324,  241,  238,  252,  238,
   324,  209,  210,  174,  238,
   324,  238,  238,  238,  238,
/* GC.C..UG */
   324,  324,  324,  324,  324,
   324,  150,  233,  238,  247,
   324,  228,  226,  238,  237,
   324,  238,  238,  238,  238,
   324,  195,  203,  238,  233,
/* GC.G..UG */
   324,  324,  324,  324,  324,
   324,  165,  238,  150,  238,
   324,  238,  238,  238,  238,
   324,  199,  238,  171,  238,
   324,  238,  238,  238,  238,
/* GC.U..UG */
   324,  324,  324,  324,  324,
   324,  238,  238,  238,  238,
   324,  238,  231,  238,  257,
   324,  238,  238,  238,  238,
   324,  238,  195,  238,  186,
/* GC.@..AU */
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
/* GC.A..AU */
   324,  324,  324,  324,  324,
   324,  221,  211,  194,  238,
   324,  241,  238,  252,  238,
   324,  209,  210,  174,  238,
   324,  238,  238,  238,  238,
/* GC.C..AU */
   324,  324,  324,  324,  324,
   324,  150,  233,  238,  247,
   324,  228,  226,  238,  237,
   324,  238,  238,  238,  238,
   324,  195,  203,  238,  233,
/* GC.G..AU */
   324,  324,  324,  324,  324,
   324,  165,  238,  150,  238,
   324,  238,  238,  238,  238,
   324,  199,  238,  171,  238,
   324,  238,  238,  238,  238,
/* GC.U..AU */
   324,  324,  324,  324,  324,
   324,  238,  238,  238,  238,
   324,  238,  231,  238,  257,
   324,  238,  238,  238,  238,
   324,  238,  195,  238,  186,
/* GC.@..UA */
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
/* GC.A..UA */
   324,  324,  324,  324,  324,
   324,  221,  211,  194,  238,
   324,  241,  238,  252,  238,
   324,  209,  210,  174,  238,
   324,  238,  238,  238,  238,
/* GC.C..UA */
   324,  324,  324,  324,  324,
   324,  150,  233,  238,  247,
   324,  228,  226,  238,  237,
   324,  238,  238,  238,  238,
   324,  195,  203,  238,  233,
/* GC.G..UA */
   324,  324,  324,  324,  324,
   324,  165,  238,  150,  238,
   324,  238,  238,  238,  238,
   324,  199,  238,  171,  238,
   324,  238,  238,  238,  238,
/* GC.U..UA */
   324,  324,  324,  324,  324,
   324,  238,  238,  238,  238,
   324,  238,  231,  238,  257,
   324,  238,  238,  238,  238,
   324,  238,  195,  238,  186,
/* GC.@.. @ */
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
/* GC.A.. @ */
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
/* GC.C.. @ */
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
/* GC.G.. @ */
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
/* GC.U.. @ */
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
/* GU.@..CG */
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
/* GU.A..CG */
   324,  324,  324,  324,  324,
   324,  221,  211,  194,  238,
   324,  241,  238,  252,  238,
   324,  209,  210,  174,  238,
   324,  238,  238,  238,  238,
/* GU.C..CG */
   324,  324,  324,  324,  324,
   324,  150,  233,  238,  247,
   324,  228,  226,  238,  237,
   324,  238,  238,  238,  238,
   324,  195,  203,  238,  233,
/* GU.G..CG */
   324,  324,  324,  324,  324,
   324,  165,  238,  150,  238,
   324,  238,  238,  238,  238,
   324,  199,  238,  171,  238,
   324,  238,  238,  238,  238,
/* GU.U..CG */
   324,  324,  324,  324,  324,
   324,  238,  238,  238,  238,
   324,  238,  231,  238,  257,
   324,  238,  238,  238,  238,
   324,  238,  195,  238,  186,
/* GU.@..GC */
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
/* GU.A..GC */
   324,  324,  324,  324,  324,
   324,  221,  211,  194,  238,
   324,  241,  238,  252,  238,
   324,  209,  210,  174,  238,
   324,  238,  238,  238,  238,
/* GU.C..GC */
   324,  324,  324,  324,  324,
   324,  150,  233,  238,  247,
   324,  228,  226,  238,  237,
   324,  238,  238,  238,  238,
   324,  195,  203,  238,  233,
/* GU.G..GC */
   324,  324,  324,  324,  324,
   324,  165,  238,  150,  238,
   324,  238,  238,  238,  238,
   324,  199,  238,  171,  238,
   324,  238,  238,  238,  238,
/* GU.U..GC */
   324,  324,  324,  324,  324,
   324,  238,  238,  238,  238,
   324,  238,  231,  238,  257,
   324,  238,  238,  238,  238,
   324,  238,  195,  238,  186,
/* GU.@..GU */
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
/* GU.A..GU */
   324,  324,  324,  324,  324,
   324,  288,  278,  261,  305,
   324,  308,  305,  319,  305,
   324,  276,  277,  241,  305,
   324,  305,  305,  305,  305,
/* GU.C..GU */
   324,  324,  324,  324,  324,
   324,  217,  300,  305,  314,
   324,  295,  293,  305,  304,
   324,  305,  305,  305,  305,
   324,  262,  270,  305,  300,
/* GU.G..GU */
   324,  324,  324,  324,  324,
   324,  232,  305,  217,  305,
   324,  305,  305,  305,  305,
   324,  266,  305,  238,  305,
   324,  305,  305,  305,  305,
/* GU.U..GU */
   324,  324,  324,  324,  324,
   324,  305,  305,  305,  305,
   324,  305,  298,  305,  324,
   324,  305,  305,  305,  305,
   324,  305,  262,  305,  253,
/* GU.@..UG */
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
/* GU.A..UG */
   324,  324,  324,  324,  324,
   324,  288,  278,  261,  305,
   324,  308,  305,  319,  305,
   324,  276,  277,  241,  305,
   324,  305,  305,  305,  305,
/* GU.C..UG */
   324,  324,  324,  324,  324,
   324,  217,  300,  305,  314,
   324,  295,  293,  305,  304,
   324,  305,  305,  305,  305,
   324,  262,  270,  305,  300,
/* GU.G..UG */
   324,  324,  324,  324,  324,
   324,  232,  305,  217,  305,
   324,  305,  305,  305,  305,
   324,  266,  305,  238,  305,
   324,  305,  305,  305,  305,
/* GU.U..UG */
   324,  324,  324,  324,  324,
   324,  305,  305,  305,  305,
   324,  305,  298,  305,  324,
   324,  305,  305,  305,  305,
   324,  305,  262,  305,  253,
/* GU.@..AU */
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
/* GU.A..AU */
   324,  324,  324,  324,  324,
   324,  288,  278,  261,  305,
   324,  308,  305,  319,  305,
   324,  276,  277,  241,  305,
   324,  305,  305,  305,  305,
/* GU.C..AU */
   324,  324,  324,  324,  324,
   324,  217,  300,  305,  314,
   324,  295,  293,  305,  304,
   324,  305,  305,  305,  305,
   324,  262,  270,  305,  300,
/* GU.G..AU */
   324,  324,  324,  324,  324,
   324,  232,  305,  217,  305,
   324,  305,  305,  305,  305,
   324,  266,  305,  238,  305,
   324,  305,  305,  305,  305,
/* GU.U..AU */
   324,  324,  324,  324,  324,
   324,  305,  305,  305,  305,
   324,  305,  298,  305,  324,
   324,  305,  305,  305,  305,
   324,  305,  262,  305,  253,
/* GU.@..UA */
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
/* GU.A..UA */
   324,  324,  324,  324,  324,
   324,  288,  278,  261,  305,
   324,  308,  305,  319,  305,
   324,  276,  277,  241,  305,
   324,  305,  305,  305,  305,
/* GU.C..UA */
   324,  324,  324,  324,  324,
   324,  217,  300,  305,  314,
   324,  295,  293,  305,  304,
   324,  305,  305,  305,  305,
   324,  262,  270,  305,  300,
/* GU.G..UA */
   324,  324,  324,  324,  324,
   324,  232,  305,  217,  305,
   324,  305,  305,  305,  305,
   324,  266,  305,  238,  305,
   324,  305,  305,  305,  305,
/* GU.U..UA */
   324,  324,  324,  324,  324,
   324,  305,  305,  305,  305,
   324,  305,  298,  305,  324,
   324,  305,  305,  305,  305,
   324,  305,  262,  305,  253,
/* GU.@.. @ */
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
/* GU.A.. @ */
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
/* GU.C.. @ */
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
/* GU.G.. @ */
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
/* GU.U.. @ */
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
/* UG.@..CG */
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
/* UG.A..CG */
   324,  324,  324,  324,  324,
   324,  221,  211,  194,  238,
   324,  241,  238,  252,  238,
   324,  209,  210,  174,  238,
   324,  238,  238,  238,  238,
/* UG.C..CG */
   324,  324,  324,  324,  324,
   324,  150,  233,  238,  247,
   324,  228,  226,  238,  237,
   324,  238,  238,  238,  238,
   324,  195,  203,  238,  233,
/* UG.G..CG */
   324,  324,  324,  324,  324,
   324,  165,  238,  150,  238,
   324,  238,  238,  238,  238,
   324,  199,  238,  171,  238,
   324,  238,  238,  238,  238,
/* UG.U..CG */
   324,  324,  324,  324,  324,
   324,  238,  238,  238,  238,
   324,  238,  231,  238,  257,
   324,  238,  238,  238,  238,
   324,  238,  195,  238,  186,
/* UG.@..GC */
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
/* UG.A..GC */
   324,  324,  324,  324,  324,
   324,  221,  211,  194,  238,
   324,  241,  238,  252,  238,
   324,  209,  210,  174,  238,
   324,  238,  238,  238,  238,
/* UG.C..GC */
   324,  324,  324,  324,  324,
   324,  150,  233,  238,  247,
   324,  228,  226,  238,  237,
   324,  238,  238,  238,  238,
   324,  195,  203,  238,  233,
/* UG.G..GC */
   324,  324,  324,  324,  324,
   324,  165,  238,  150,  238,
   324,  238,  238,  238,  238,
   324,  199,  238,  171,  238,
   324,  238,  238,  238,  238,
/* UG.U..GC */
   324,  324,  324,  324,  324,
   324,  238,  238,  238,  238,
   324,  238,  231,  238,  257,
   324,  238,  238,  238,  238,
   324,  238,  195,  238,  186,
/* UG.@..GU */
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
/* UG.A..GU */
   324,  324,  324,  324,  324,
   324,  288,  278,  261,  305,
   324,  308,  305,  319,  305,
   324,  276,  277,  241,  305,
   324,  305,  305,  305,  305,
/* UG.C..GU */
   324,  324,  324,  324,  324,
   324,  217,  300,  305,  314,
   324,  295,  293,  305,  304,
   324,  305,  305,  305,  305,
   324,  262,  270,  305,  300,
/* UG.G..GU */
   324,  324,  324,  324,  324,
   324,  232,  305,  217,  305,
   324,  305,  305,  305,  305,
   324,  266,  305,  238,  305,
   324,  305,  305,  305,  305,
/* UG.U..GU */
   324,  324,  324,  324,  324,
   324,  305,  305,  305,  305,
   324,  305,  298,  305,  324,
   324,  305,  305,  305,  305,
   324,  305,  262,  305,  253,
/* UG.@..UG */
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
/* UG.A..UG */
   324,  324,  324,  324,  324,
   324,  288,  278,  261,  305,
   324,  308,  305,  319,  305,
   324,  276,  277,  241,  305,
   324,  305,  305,  305,  305,
/* UG.C..UG */
   324,  324,  324,  324,  324,
   324,  217,  300,  305,  314,
   324,  295,  293,  305,  304,
   324,  305,  305,  305,  305,
   324,  262,  270,  305,  300,
/* UG.G..UG */
   324,  324,  324,  324,  324,
   324,  232,  305,  217,  305,
   324,  305,  305,  305,  305,
   324,  266,  305,  238,  305,
   324,  305,  305,  305,  305,
/* UG.U..UG */
   324,  324,  324,  324,  324,
   324,  305,  305,  305,  305,
   324,  305,  298,  305,  324,
   324,  305,  305,  305,  305,
   324,  305,  262,  305,  253,
/* UG.@..AU */
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
/* UG.A..AU */
   324,  324,  324,  324,  324,
   324,  288,  278,  261,  305,
   324,  308,  305,  319,  305,
   324,  276,  277,  241,  305,
   324,  305,  305,  305,  305,
/* UG.C..AU */
   324,  324,  324,  324,  324,
   324,  217,  300,  305,  314,
   324,  295,  293,  305,  304,
   324,  305,  305,  305,  305,
   324,  262,  270,  305,  300,
/* UG.G..AU */
   324,  324,  324,  324,  324,
   324,  232,  305,  217,  305,
   324,  305,  305,  305,  305,
   324,  266,  305,  238,  305,
   324,  305,  305,  305,  305,
/* UG.U..AU */
   324,  324,  324,  324,  324,
   324,  305,  305,  305,  305,
   324,  305,  298,  305,  324,
   324,  305,  305,  305,  305,
   324,  305,  262,  305,  253,
/* UG.@..UA */
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
/* UG.A..UA */
   324,  324,  324,  324,  324,
   324,  288,  278,  261,  305,
   324,  308,  305,  319,  305,
   324,  276,  277,  241,  305,
   324,  305,  305,  305,  305,
/* UG.C..UA */
   324,  324,  324,  324,  324,
   324,  217,  300,  305,  314,
   324,  295,  293,  305,  304,
   324,  305,  305,  305,  305,
   324,  262,  270,  305,  300,
/* UG.G..UA */
   324,  324,  324,  324,  324,
   324,  232,  305,  217,  305,
   324,  305,  305,  305,  305,
   324,  266,  305,  238,  305,
   324,  305,  305,  305,  305,
/* UG.U..UA */
   324,  324,  324,  324,  324,
   324,  305,  305,  305,  305,
   324,  305,  298,  305,  324,
   324,  305,  305,  305,  305,
   324,  305,  262,  305,  253,
/* UG.@.. @ */
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
/* UG.A.. @ */
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
/* UG.C.. @ */
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
/* UG.G.. @ */
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
/* UG.U.. @ */
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
/* AU.@..CG */
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
/* AU.A..CG */
   324,  324,  324,  324,  324,
   324,  221,  211,  194,  238,
   324,  241,  238,  252,  238,
   324,  209,  210,  174,  238,
   324,  238,  238,  238,  238,
/* AU.C..CG */
   324,  324,  324,  324,  324,
   324,  150,  233,  238,  247,
   324,  228,  226,  238,  237,
   324,  238,  238,  238,  238,
   324,  195,  203,  238,  233,
/* AU.G..CG */
   324,  324,  324,  324,  324,
   324,  165,  238,  150,  238,
   324,  238,  238,  238,  238,
   324,  199,  238,  171,  238,
   324,  238,  238,  238,  238,
/* AU.U..CG */
   324,  324,  324,  324,  324,
   324,  238,  238,  238,  238,
   324,  238,  231,  238,  257,
   324,  238,  238,  238,  238,
   324,  238,  195,  238,  186,
/* AU.@..GC */
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
/* AU.A..GC */
   324,  324,  324,  324,  324,
   324,  221,  211,  194,  238,
   324,  241,  238,  252,  238,
   324,  209,  210,  174,  238,
   324,  238,  238,  238,  238,
/* AU.C..GC */
   324,  324,  324,  324,  324,
   324,  150,  233,  238,  247,
   324,  228,  226,  238,  237,
   324,  238,  238,  238,  238,
   324,  195,  203,  238,  233,
/* AU.G..GC */
   324,  324,  324,  324,  324,
   324,  165,  238,  150,  238,
   324,  238,  238,  238,  238,
   324,  199,  238,  171,  238,
   324,  238,  238,  238,  238,
/* AU.U..GC */
   324,  324,  324,  324,  324,
   324,  238,  238,  238,  238,
   324,  238,  231,  238,  257,
   324,  238,  238,  238,  238,
   324,  238,  195,  238,  186,
/* AU.@..GU */
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
/* AU.A..GU */
   324,  324,  324,  324,  324,
   324,  288,  278,  261,  305,
   324,  308,  305,  319,  305,
   324,  276,  277,  241,  305,
   324,  305,  305,  305,  305,
/* AU.C..GU */
   324,  324,  324,  324,  324,
   324,  217,  300,  305,  314,
   324,  295,  293,  305,  304,
   324,  305,  305,  305,  305,
   324,  262,  270,  305,  300,
/* AU.G..GU */
   324,  324,  324,  324,  324,
   324,  232,  305,  217,  305,
   324,  305,  305,  305,  305,
   324,  266,  305,  238,  305,
   324,  305,  305,  305,  305,
/* AU.U..GU */
   324,  324,  324,  324,  324,
   324,  305,  305,  305,  305,
   324,  305,  298,  305,  324,
   324,  305,  305,  305,  305,
   324,  305,  262,  305,  253,
/* AU.@..UG */
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
/* AU.A..UG */
   324,  324,  324,  324,  324,
   324,  288,  278,  261,  305,
   324,  308,  305,  319,  305,
   324,  276,  277,  241,  305,
   324,  305,  305,  305,  305,
/* AU.C..UG */
   324,  324,  324,  324,  324,
   324,  217,  300,  305,  314,
   324,  295,  293,  305,  304,
   324,  305,  305,  305,  305,
   324,  262,  270,  305,  300,
/* AU.G..UG */
   324,  324,  324,  324,  324,
   324,  232,  305,  217,  305,
   324,  305,  305,  305,  305,
   324,  266,  305,  238,  305,
   324,  305,  305,  305,  305,
/* AU.U..UG */
   324,  324,  324,  324,  324,
   324,  305,  305,  305,  305,
   324,  305,  298,  305,  324,
   324,  305,  305,  305,  305,
   324,  305,  262,  305,  253,
/* AU.@..AU */
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
/* AU.A..AU */
   324,  324,  324,  324,  324,
   324,  288,  278,  261,  305,
   324,  308,  305,  319,  305,
   324,  276,  277,  241,  305,
   324,  305,  305,  305,  305,
/* AU.C..AU */
   324,  324,  324,  324,  324,
   324,  217,  300,  305,  314,
   324,  295,  293,  305,  304,
   324,  305,  305,  305,  305,
   324,  262,  270,  305,  300,
/* AU.G..AU */
   324,  324,  324,  324,  324,
   324,  232,  305,  217,  305,
   324,  305,  305,  305,  305,
   324,  266,  305,  238,  305,
   324,  305,  305,  305,  305,
/* AU.U..AU */
   324,  324,  324,  324,  324,
   324,  305,  305,  305,  305,
   324,  305,  298,  305,  324,
   324,  305,  305,  305,  305,
   324,  305,  262,  305,  253,
/* AU.@..UA */
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
/* AU.A..UA */
   324,  324,  324,  324,  324,
   324,  288,  278,  261,  305,
   324,  308,  305,  319,  305,
   324,  276,  277,  241,  305,
   324,  305,  305,  305,  305,
/* AU.C..UA */
   324,  324,  324,  324,  324,
   324,  217,  300,  305,  314,
   324,  295,  293,  305,  304,
   324,  305,  305,  305,  305,
   324,  262,  270,  305,  300,
/* AU.G..UA */
   324,  324,  324,  324,  324,
   324,  232,  305,  217,  305,
   324,  305,  305,  305,  305,
   324,  266,  305,  238,  305,
   324,  305,  305,  305,  305,
/* AU.U..UA */
   324,  324,  324,  324,  324,
   324,  305,  305,  305,  305,
   324,  305,  298,  305,  324,
   324,  305,  305,  305,  305,
   324,  305,  262,  305,  253,
/* AU.@.. @ */
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
/* AU.A.. @ */
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
/* AU.C.. @ */
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
/* AU.G.. @ */
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
/* AU.U.. @ */
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
/* UA.@..CG */
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
/* UA.A..CG */
   324,  324,  324,  324,  324,
   324,  221,  211,  194,  238,
   324,  241,  238,  252,  238,
   324,  209,  210,  174,  238,
   324,  238,  238,  238,  238,
/* UA.C..CG */
   324,  324,  324,  324,  324,
   324,  150,  233,  238,  247,
   324,  228,  226,  238,  237,
   324,  238,  238,  238,  238,
   324,  195,  203,  238,  233,
/* UA.G..CG */
   324,  324,  324,  324,  324,
   324,  165,  238,  150,  238,
   324,  238,  238,  238,  238,
   324,  199,  238,  171,  238,
   324,  238,  238,  238,  238,
/* UA.U..CG */
   324,  324,  324,  324,  324,
   324,  238,  238,  238,  238,
   324,  238,  231,  238,  257,
   324,  238,  238,  238,  238,
   324,  238,  195,  238,  186,
/* UA.@..GC */
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
/* UA.A..GC */
   324,  324,  324,  324,  324,
   324,  221,  211,  194,  238,
   324,  241,  238,  252,  238,
   324,  209,  210,  174,  238,
   324,  238,  238,  238,  238,
/* UA.C..GC */
   324,  324,  324,  324,  324,
   324,  150,  233,  238,  247,
   324,  228,  226,  238,  237,
   324,  238,  238,  238,  238,
   324,  195,  203,  238,  233,
/* UA.G..GC */
   324,  324,  324,  324,  324,
   324,  165,  238,  150,  238,
   324,  238,  238,  238,  238,
   324,  199,  238,  171,  238,
   324,  238,  238,  238,  238,
/* UA.U..GC */
   324,  324,  324,  324,  324,
   324,  238,  238,  238,  238,
   324,  238,  231,  238,  257,
   324,  238,  238,  238,  238,
   324,  238,  195,  238,  186,
/* UA.@..GU */
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
/* UA.A..GU */
   324,  324,  324,  324,  324,
   324,  288,  278,  261,  305,
   324,  308,  305,  319,  305,
   324,  276,  277,  241,  305,
   324,  305,  305,  305,  305,
/* UA.C..GU */
   324,  324,  324,  324,  324,
   324,  217,  300,  305,  314,
   324,  295,  293,  305,  304,
   324,  305,  305,  305,  305,
   324,  262,  270,  305,  300,
/* UA.G..GU */
   324,  324,  324,  324,  324,
   324,  232,  305,  217,  305,
   324,  305,  305,  305,  305,
   324,  266,  305,  238,  305,
   324,  305,  305,  305,  305,
/* UA.U..GU */
   324,  324,  324,  324,  324,
   324,  305,  305,  305,  305,
   324,  305,  298,  305,  324,
   324,  305,  305,  305,  305,
   324,  305,  262,  305,  253,
/* UA.@..UG */
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
/* UA.A..UG */
   324,  324,  324,  324,  324,
   324,  288,  278,  261,  305,
   324,  308,  305,  319,  305,
   324,  276,  277,  241,  305,
   324,  305,  305,  305,  305,
/* UA.C..UG */
   324,  324,  324,  324,  324,
   324,  217,  300,  305,  314,
   324,  295,  293,  305,  304,
   324,  305,  305,  305,  305,
   324,  262,  270,  305,  300,
/* UA.G..UG */
   324,  324,  324,  324,  324,
   324,  232,  305,  217,  305,
   324,  305,  305,  305,  305,
   324,  266,  305,  238,  305,
   324,  305,  305,  305,  305,
/* UA.U..UG */
   324,  324,  324,  324,  324,
   324,  305,  305,  305,  305,
   324,  305,  298,  305,  324,
   324,  305,  305,  305,  305,
   324,  305,  262,  305,  253,
/* UA.@..AU */
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
/* UA.A..AU */
   324,  324,  324,  324,  324,
   324,  288,  278,  261,  305,
   324,  308,  305,  319,  305,
   324,  276,  277,  241,  305,
   324,  305,  305,  305,  305,
/* UA.C..AU */
   324,  324,  324,  324,  324,
   324,  217,  300,  305,  314,
   324,  295,  293,  305,  304,
   324,  305,  305,  305,  305,
   324,  262,  270,  305,  300,
/* UA.G..AU */
   324,  324,  324,  324,  324,
   324,  232,  305,  217,  305,
   324,  305,  305,  305,  305,
   324,  266,  305,  238,  305,
   324,  305,  305,  305,  305,
/* UA.U..AU */
   324,  324,  324,  324,  324,
   324,  305,  305,  305,  305,
   324,  305,  298,  305,  324,
   324,  305,  305,  305,  305,
   324,  305,  262,  305,  253,
/* UA.@..UA */
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
/* UA.A..UA */
   324,  324,  324,  324,  324,
   324,  288,  278,  261,  305,
   324,  308,  305,  319,  305,
   324,  276,  277,  241,  305,
   324,  305,  305,  305,  305,
/* UA.C..UA */
   324,  324,  324,  324,  324,
   324,  217,  300,  305,  314,
   324,  295,  293,  305,  304,
   324,  305,  305,  305,  305,
   324,  262,  270,  305,  300,
/* UA.G..UA */
   324,  324,  324,  324,  324,
   324,  232,  305,  217,  305,
   324,  305,  305,  305,  305,
   324,  266,  305,  238,  305,
   324,  305,  305,  305,  305,
/* UA.U..UA */
   324,  324,  324,  324,  324,
   324,  305,  305,  305,  305,
   324,  305,  298,  305,  324,
   324,  305,  305,  305,  305,
   324,  305,  262,  305,  253,
/* UA.@.. @ */
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
/* UA.A.. @ */
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
/* UA.C.. @ */
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
/* UA.G.. @ */
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
/* UA.U.. @ */
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
/*  @.@..CG */
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
/*  @.A..CG */
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
/*  @.C..CG */
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
/*  @.G..CG */
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
/*  @.U..CG */
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
/*  @.@..GC */
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
/*  @.A..GC */
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
/*  @.C..GC */
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
/*  @.G..GC */
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
/*  @.U..GC */
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
/*  @.@..GU */
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
/*  @.A..GU */
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
/*  @.C..GU */
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
/*  @.G..GU */
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
/*  @.U..GU */
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
/*  @.@..UG */
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
/*  @.A..UG */
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
/*  @.C..UG */
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
/*  @.G..UG */
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
/*  @.U..UG */
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
/*  @.@..AU */
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
/*  @.A..AU */
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
/*  @.C..AU */
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
/*  @.G..AU */
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
/*  @.U..AU */
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
/*  @.@..UA */
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
/*  @.A..UA */
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
/*  @.C..UA */
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
/*  @.G..UA */
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
/*  @.U..UA */
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
/*  @.@.. @ */
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
/*  @.A.. @ */
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
/*  @.C.. @ */
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
/*  @.G.. @ */
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
/*  @.U.. @ */
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324,
   324,  324,  324,  324,  324
};

/* int22_energies */
static int int22_37a[] = {
/* CG.AA..CG */
    99,  158,   70,  242,
   176,  188,  127,  242,
    63,  102,   14,  242,
   242,  242,  242,  242,
/* CG.AC..CG */
   158,  123,  109,  242,
   145,  184,  143,  242,
   242,  242,  242,  242,
   118,  157,  116,  242,
/* CG.AG..CG */
    70,  109,   -1,  242,
   242,  242,  242,  242,
   131,  170,   82,  242,
   127,  119,  153,  242,
/* CG.AU..CG */
   242,  242,  242,  242,
   145,  184,  143,  242,
   189,  181,  215,  242,
    99,   91,  125,  242,
/* CG.CA..CG */
   176,  145,  242,  145,
   158,  202,  242,  202,
   120,  136,  242,  136,
   242,  242,  242,  242,
/* CG.CC..CG */
   188,  184,  242,  184,
   202,  151,  242,  171,
   242,  242,  242,  242,
   175,  144,  242,  144,
/* CG.CG..CG */
   127,  143,  242,  143,
   242,  242,  242,  242,
   188,  157,  242,  157,
   137,  153,  242,  153,
/* CG.CU..CG */
   242,  242,  242,  242,
   202,  171,  242,  150,
   199,  215,  242,  215,
   109,   78,  242,   78,
/* CG.GA..CG */
    63,  242,  131,  189,
   120,  242,  188,  199,
   -15,  242,   75,  208,
   242,  242,  242,  242,
/* CG.GC..CG */
   102,  242,  170,  181,
   136,  242,  157,  215,
   242,  242,  242,  242,
   109,  242,  130,  188,
/* CG.GG..CG */
    14,  242,   82,  215,
   242,  242,  242,  242,
    75,  242,  123,  201,
   146,  242,  139,  103,
/* CG.GU..CG */
   242,  242,  242,  242,
   136,  242,  157,  215,
   208,  242,  201,  144,
   118,  242,  111,  197,
/* CG.UA..CG */
   242,  118,  127,   99,
   242,  175,  137,  109,
   242,  109,  146,  118,
   242,  242,  242,  242,
/* CG.UC..CG */
   242,  157,  119,   91,
   242,  144,  153,   78,
   242,  242,  242,  242,
   242,   97,  126,   51,
/* CG.UG..CG */
   242,  116,  153,  125,
   242,  242,  242,  242,
   242,  130,  139,  111,
   242,  126,   21,  135,
/* CG.UU..CG */
   242,  242,  242,  242,
   242,  144,  153,   78,
   242,  188,  103,  197,
   242,   51,  135,  -36,
/* CG.AA..GC */
   141,  180,   92,  242,
   151,  163,  102,  242,
    45,   84,   -4,  242,
   242,  242,  242,  242,
/* CG.AC..GC */
   154,  166,  105,  242,
   130,  169,  128,  242,
   242,  242,  242,  242,
   131,  170,  129,  242,
/* CG.AG..GC */
    66,  105,   17,  242,
   242,  242,  242,  242,
   105,  144,   56,  242,
   158,  150,  184,  242,
/* CG.AU..GC */
   242,  242,  242,  242,
   106,  145,  104,  242,
   101,   93,  127,  242,
    92,   84,  118,  242,
/* CG.CA..GC */
   198,  167,  242,  167,
   181,  177,  242,  177,
   102,  118,  242,  118,
   242,  242,  242,  242,
/* CG.CC..GC */
   184,  180,  242,  180,
   187,  156,  242,  156,
   242,  242,  242,  242,
   188,  157,  242,  157,
/* CG.CG..GC */
   123,  139,  242,  139,
   242,  242,  242,  242,
   162,  131,  242,  131,
   168,  184,  242,  184,
/* CG.CU..GC */
   242,  242,  242,  242,
   163,  132,  242,  132,
   111,  127,  242,  127,
   102,   71,  242,   71,
/* CG.GA..GC */
    85,  242,  153,  211,
    95,  242,  163,  174,
   -11,  242,   57,  190,
   242,  242,  242,  242,
/* CG.GC..GC */
    98,  242,  166,  177,
   121,  242,  142,  200,
   242,  242,  242,  242,
   122,  242,  143,  201,
/* CG.GG..GC */
    10,  242,   78,  211,
   242,  242,  242,  242,
    49,  242,  117,  175,
   177,  242,  170,  134,
/* CG.GU..GC */
   242,  242,  242,  242,
    97,  242,  118,  176,
   120,  242,  113,   77,
   111,  242,  104,  190,
/* CG.UA..GC */
   242,  140,  149,  121,
   242,  150,  112,   84,
   242,   91,  128,  100,
   242,  242,  242,  242,
/* CG.UC..GC */
   242,  153,  115,   87,
   242,  129,  138,   63,
   242,  242,  242,  242,
   242,  130,  139,   64,
/* CG.UG..GC */
   242,  112,  149,  121,
   242,  242,  242,  242,
   242,  104,  113,   85,
   242,  157,   72,  166,
/* CG.UU..GC */
   242,  242,  242,  242,
   242,  105,  114,   39,
   242,  100,   15,  109,
   242,   44,  128,  -22,
/* CG.AA..GU */
   150,  189,  101,  242,
   217,  229,  168,  242,
    95,  134,   46,  242,
   242,  242,  242,  242,
/* CG.AC..GU */
   149,  161,  100,  242,
   134,  173,  132,  242,
   242,  242,  242,  242,
   152,  191,  150,  242,
/* CG.AG..GU */
   163,  202,  114,  242,
   242,  242,  242,  242,
   132,  171,   83,  242,
   227,  219,  253,  242,
/* CG.AU..GU */
   242,  242,  242,  242,
   114,  153,  112,  242,
   219,  211,  245,  242,
   119,  111,  145,  242,
/* CG.CA..GU */
   207,  176,  242,  176,
   247,  243,  242,  243,
   152,  168,  242,  168,
   242,  242,  242,  242,
/* CG.CC..GU */
   179,  175,  242,  175,
   191,  160,  242,  160,
   242,  242,  242,  242,
   209,  178,  242,  178,
/* CG.CG..GU */
   220,  236,  242,  236,
   242,  242,  242,  242,
   189,  158,  242,  158,
   237,  253,  242,  253,
/* CG.CU..GU */
   242,  242,  242,  242,
   171,  140,  242,  140,
   229,  245,  242,  245,
   129,   98,  242,   98,
/* CG.GA..GU */
    94,  242,  162,  220,
   161,  242,  229,  240,
    39,  242,  107,  240,
   242,  242,  242,  242,
/* CG.GC..GU */
    93,  242,  161,  172,
   125,  242,  146,  204,
   242,  242,  242,  242,
   143,  242,  164,  222,
/* CG.GG..GU */
   107,  242,  175,  308,
   242,  242,  242,  242,
    76,  242,  144,  202,
   246,  242,  239,  203,
/* CG.GU..GU */
   242,  242,  242,  242,
   105,  242,  126,  184,
   238,  242,  231,  195,
   138,  242,  131,  217,
/* CG.UA..GU */
   242,  149,  158,  130,
   242,  216,  178,  150,
   242,  141,  178,  150,
   242,  242,  242,  242,
/* CG.UC..GU */
   242,  148,  110,   82,
   242,  133,  142,   67,
   242,  242,  242,  242,
   242,  151,  160,   85,
/* CG.UG..GU */
   242,  209,  246,  218,
   242,  242,  242,  242,
   242,  131,  140,  112,
   242,  226,  141,  235,
/* CG.UU..GU */
   242,  242,  242,  242,
   242,  113,  122,   47,
   242,  218,  133,  227,
   242,   71,  155,    5,
/* CG.AA..UG */
   202,  241,  153,  242,
   179,  191,  130,  242,
    85,  124,   36,  242,
   242,  242,  242,  242,
/* CG.AC..UG */
   154,  166,  105,  242,
   162,  201,  160,  242,
   242,  242,  242,  242,
   168,  207,  166,  242,
/* CG.AG..UG */
   161,  200,  112,  242,
   242,  242,  242,  242,
   172,  211,  123,  242,
   232,  224,  258,  242,
/* CG.AU..UG */
   242,  242,  242,  242,
   190,  229,  188,  242,
   249,  241,  275,  242,
   114,  106,  140,  242,
/* CG.CA..UG */
   259,  228,  242,  228,
   209,  205,  242,  205,
   142,  158,  242,  158,
   242,  242,  242,  242,
/* CG.CC..UG */
   184,  180,  242,  180,
   219,  188,  242,  188,
   242,  242,  242,  242,
   225,  194,  242,  194,
/* CG.CG..UG */
   218,  234,  242,  234,
   242,  242,  242,  242,
   229,  198,  242,  198,
   242,  258,  242,  258,
/* CG.CU..UG */
   242,  242,  242,  242,
   247,  216,  242,  216,
   259,  275,  242,  275,
   124,   93,  242,   93,
/* CG.GA..UG */
   146,  242,  214,  272,
   123,  242,  191,  202,
    29,  242,   97,  230,
   242,  242,  242,  242,
/* CG.GC..UG */
    98,  242,  166,  177,
   153,  242,  174,  232,
   242,  242,  242,  242,
   159,  242,  180,  238,
/* CG.GG..UG */
   105,  242,  173,  306,
   242,  242,  242,  242,
   116,  242,  184,  242,
   251,  242,  244,  208,
/* CG.GU..UG */
   242,  242,  242,  242,
   181,  242,  202,  260,
   268,  242,  261,  225,
   133,  242,  126,  212,
/* CG.UA..UG */
   242,  201,  210,  182,
   242,  178,  140,  112,
   242,  131,  168,  140,
   242,  242,  242,  242,
/* CG.UC..UG */
   242,  153,  115,   87,
   242,  161,  170,   95,
   242,  242,  242,  242,
   242,  167,  176,  101,
/* CG.UG..UG */
   242,  207,  244,  216,
   242,  242,  242,  242,
   242,  171,  180,  152,
   242,  231,  146,  240,
/* CG.UU..UG */
   242,  242,  242,  242,
   242,  189,  198,  123,
   242,  248,  163,  257,
   242,   66,  150,    0,
/* CG.AA..AU */
   150,  189,  101,  242,
   217,  229,  168,  242,
    95,  134,   46,  242,
   242,  242,  242,  242,
/* CG.AC..AU */
   149,  161,  100,  242,
   134,  173,  132,  242,
   242,  242,  242,  242,
   152,  191,  150,  242,
/* CG.AG..AU */
   163,  202,  114,  242,
   242,  242,  242,  242,
   132,  171,   83,  242,
   227,  219,  253,  242,
/* CG.AU..AU */
   242,  242,  242,  242,
   114,  153,  112,  242,
   219,  211,  245,  242,
   119,  111,  145,  242,
/* CG.CA..AU */
   207,  176,  242,  176,
   247,  243,  242,  243,
   152,  168,  242,  168,
   242,  242,  242,  242,
/* CG.CC..AU */
   179,  175,  242,  175,
   191,  160,  242,  160,
   242,  242,  242,  242,
   209,  178,  242,  178,
/* CG.CG..AU */
   220,  236,  242,  236,
   242,  242,  242,  242,
   189,  158,  242,  158,
   237,  253,  242,  253,
/* CG.CU..AU */
   242,  242,  242,  242,
   171,  140,  242,  140,
   229,  245,  242,  245,
   129,   98,  242,   98,
/* CG.GA..AU */
    94,  242,  162,  220,
   161,  242,  229,  240,
    39,  242,  107,  240,
   242,  242,  242,  242,
/* CG.GC..AU */
    93,  242,  161,  172,
   125,  242,  146,  204,
   242,  242,  242,  242,
   143,  242,  164,  222,
/* CG.GG..AU */
   107,  242,  175,  308,
   242,  242,  242,  242,
    76,  242,  144,  202,
   246,  242,  239,  203,
/* CG.GU..AU */
   242,  242,  242,  242,
   105,  242,  126,  184,
   238,  242,  231,  195,
   138,  242,  131,  217,
/* CG.UA..AU */
   242,  149,  158,  130,
   242,  216,  178,  150,
   242,  141,  178,  150,
   242,  242,  242,  242,
/* CG.UC..AU */
   242,  148,  110,   82,
   242,  133,  142,   67,
   242,  242,  242,  242,
   242,  151,  160,   85,
/* CG.UG..AU */
   242,  209,  246,  218,
   242,  242,  242,  242,
   242,  131,  140,  112,
   242,  226,  141,  235,
/* CG.UU..AU */
   242,  242,  242,  242,
   242,  113,  122,   47,
   242,  218,  133,  227,
   242,   71,  155,    5,
/* CG.AA..UA */
   202,  241,  153,  242,
   179,  191,  130,  242,
    85,  124,   36,  242,
   242,  242,  242,  242,
/* CG.AC..UA */
   154,  166,  105,  242,
   162,  201,  160,  242,
   242,  242,  242,  242,
   168,  207,  166,  242,
/* CG.AG..UA */
   161,  200,  112,  242,
   242,  242,  242,  242,
   172,  211,  123,  242,
   232,  224,  258,  242,
/* CG.AU..UA */
   242,  242,  242,  242,
   190,  229,  188,  242,
   249,  241,  275,  242,
   114,  106,  140,  242,
/* CG.CA..UA */
   259,  228,  242,  228,
   209,  205,  242,  205,
   142,  158,  242,  158,
   242,  242,  242,  242,
/* CG.CC..UA */
   184,  180,  242,  180,
   219,  188,  242,  188,
   242,  242,  242,  242,
   225,  194,  242,  194,
/* CG.CG..UA */
   218,  234,  242,  234,
   242,  242,  242,  242,
   229,  198,  242,  198,
   242,  258,  242,  258,
/* CG.CU..UA */
   242,  242,  242,  242,
   247,  216,  242,  216,
   259,  275,  242,  275,
   124,   93,  242,   93,
/* CG.GA..UA */
   146,  242,  214,  272,
   123,  242,  191,  202,
    29,  242,   97,  230,
   242,  242,  242,  242,
/* CG.GC..UA */
    98,  242,  166,  177,
   153,  242,  174,  232,
   242,  242,  242,  242,
   159,  242,  180,  238,
/* CG.GG..UA */
   105,  242,  173,  306,
   242,  242,  242,  242,
   116,  242,  184,  242,
   251,  242,  244,  208,
/* CG.GU..UA */
   242,  242,  242,  242,
   181,  242,  202,  260,
   268,  242,  261,  225,
   133,  242,  126,  212,
/* CG.UA..UA */
   242,  201,  210,  182,
   242,  178,  140,  112,
   242,  131,  168,  140,
   242,  242,  242,  242,
/* CG.UC..UA */
   242,  153,  115,   87,
   242,  161,  170,   95,
   242,  242,  242,  242,
   242,  167,  176,  101,
/* CG.UG..UA */
   242,  207,  244,  216,
   242,  242,  242,  242,
   242,  171,  180,  152,
   242,  231,  146,  240,
/* CG.UU..UA */
   242,  242,  242,  242,
   242,  189,  198,  123,
   242,  248,  163,  257,
   242,   66,  150,    0,
/* CG.AA.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/* CG.AC.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/* CG.AG.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/* CG.AU.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/* CG.CA.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/* CG.CC.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/* CG.CG.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/* CG.CU.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/* CG.GA.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/* CG.GC.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/* CG.GG.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/* CG.GU.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/* CG.UA.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/* CG.UC.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/* CG.UG.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/* CG.UU.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/* GC.AA..CG */
   141,  154,   66,  242,
   198,  184,  123,  242,
    85,   98,   10,  242,
   242,  242,  242,  242,
/* GC.AC..CG */
   180,  166,  105,  242,
   167,  180,  139,  242,
   242,  242,  242,  242,
   140,  153,  112,  242,
/* GC.AG..CG */
    92,  105,   17,  242,
   242,  242,  242,  242,
   153,  166,   78,  242,
   149,  115,  149,  242,
/* GC.AU..CG */
   242,  242,  242,  242,
   167,  180,  139,  242,
   211,  177,  211,  242,
   121,   87,  121,  242,
/* GC.CA..CG */
   151,  130,  242,  106,
   181,  187,  242,  163,
    95,  121,  242,   97,
   242,  242,  242,  242,
/* GC.CC..CG */
   163,  169,  242,  145,
   177,  156,  242,  132,
   242,  242,  242,  242,
   150,  129,  242,  105,
/* GC.CG..CG */
   102,  128,  242,  104,
   242,  242,  242,  242,
   163,  142,  242,  118,
   112,  138,  242,  114,
/* GC.CU..CG */
   242,  242,  242,  242,
   177,  156,  242,  132,
   174,  200,  242,  176,
    84,   63,  242,   39,
/* GC.GA..CG */
    45,  242,  105,  101,
   102,  242,  162,  111,
   -11,  242,   49,  120,
   242,  242,  242,  242,
/* GC.GC..CG */
    84,  242,  144,   93,
   118,  242,  131,  127,
   242,  242,  242,  242,
    91,  242,  104,  100,
/* GC.GG..CG */
    -4,  242,   56,  127,
   242,  242,  242,  242,
    57,  242,  117,  113,
   128,  242,  113,   15,
/* GC.GU..CG */
   242,  242,  242,  242,
   118,  242,  131,  127,
   190,  242,  175,   77,
   100,  242,   85,  109,
/* GC.UA..CG */
   242,  131,  158,   92,
   242,  188,  168,  102,
   242,  122,  177,  111,
   242,  242,  242,  242,
/* GC.UC..CG */
   242,  170,  150,   84,
   242,  157,  184,   71,
   242,  242,  242,  242,
   242,  130,  157,   44,
/* GC.UG..CG */
   242,  129,  184,  118,
   242,  242,  242,  242,
   242,  143,  170,  104,
   242,  139,   72,  128,
/* GC.UU..CG */
   242,  242,  242,  242,
   242,  157,  184,   71,
   242,  201,  134,  190,
   242,   64,  166,  -22,
/* GC.AA..GC */
   142,  176,   88,  242,
   173,  159,   98,  242,
    67,   80,   -8,  242,
   242,  242,  242,  242,
/* GC.AC..GC */
   176,  115,  101,  242,
   152,  165,  124,  242,
   242,  242,  242,  242,
   153,  166,  125,  242,
/* GC.AG..GC */
    88,  101,   -8,  242,
   242,  242,  242,  242,
   127,  140,   52,  242,
   180,  146,  180,  242,
/* GC.AU..GC */
   242,  242,  242,  242,
   128,  141,  100,  242,
   123,   89,  123,  242,
   114,   80,  114,  242,
/* GC.CA..GC */
   173,  152,  242,  128,
   109,  162,  242,  138,
    77,  103,  242,   79,
   242,  242,  242,  242,
/* GC.CC..GC */
   159,  165,  242,  141,
   162,  121,  242,  117,
   242,  242,  242,  242,
   163,  142,  242,  118,
/* GC.CG..GC */
    98,  124,  242,  100,
   242,  242,  242,  242,
   137,  116,  242,   92,
   143,  169,  242,  145,
/* GC.CU..GC */
   242,  242,  242,  242,
   138,  117,  242,   72,
    86,  112,  242,   88,
    77,   56,  242,   32,
/* GC.GA..GC */
    67,  242,  127,  123,
    77,  242,  137,   86,
   -51,  242,   31,  102,
   242,  242,  242,  242,
/* GC.GC..GC */
    80,  242,  140,   89,
   103,  242,  116,  112,
   242,  242,  242,  242,
   104,  242,  117,  113,
/* GC.GG..GC */
    -8,  242,   52,  123,
   242,  242,  242,  242,
    31,  242,   71,   87,
   159,  242,  144,   46,
/* GC.GU..GC */
   242,  242,  242,  242,
    79,  242,   92,   88,
   102,  242,   87,  -33,
    93,  242,   78,  102,
/* GC.UA..GC */
   242,  153,  180,  114,
   242,  163,  143,   77,
   242,  104,  159,   93,
   242,  242,  242,  242,
/* GC.UC..GC */
   242,  166,  146,   80,
   242,  142,  169,   56,
   242,  242,  242,  242,
   242,  123,  170,   57,
/* GC.UG..GC */
   242,  125,  180,  114,
   242,  242,  242,  242,
   242,  117,  144,   78,
   242,  170,   83,  159,
/* GC.UU..GC */
   242,  242,  242,  242,
   242,  118,  145,   32,
   242,  113,   46,  102,
   242,   57,  159,  -50,
/* GC.AA..GU */
   172,  185,   97,  242,
   239,  225,  164,  242,
   117,  130,   42,  242,
   242,  242,  242,  242,
/* GC.AC..GU */
   171,  157,   96,  242,
   156,  169,  128,  242,
   242,  242,  242,  242,
   174,  187,  146,  242,
/* GC.AG..GU */
   185,  198,  110,  242,
   242,  242,  242,  242,
   154,  167,   79,  242,
   249,  215,  249,  242,
/* GC.AU..GU */
   242,  242,  242,  242,
   136,  149,  108,  242,
   241,  207,  241,  242,
   141,  107,  141,  242,
/* GC.CA..GU */
   182,  161,  242,  137,
   222,  228,  242,  204,
   127,  153,  242,  129,
   242,  242,  242,  242,
/* GC.CC..GU */
   154,  160,  242,  136,
   166,  145,  242,  121,
   242,  242,  242,  242,
   184,  163,  242,  139,
/* GC.CG..GU */
   195,  221,  242,  197,
   242,  242,  242,  242,
   164,  143,  242,  119,
   212,  238,  242,  214,
/* GC.CU..GU */
   242,  242,  242,  242,
   146,  125,  242,  101,
   204,  230,  242,  206,
   104,   83,  242,   59,
/* GC.GA..GU */
    76,  242,  136,  132,
   143,  242,  203,  152,
    21,  242,   81,  152,
   242,  242,  242,  242,
/* GC.GC..GU */
    75,  242,  135,   84,
   107,  242,  120,  116,
   242,  242,  242,  242,
   125,  242,  138,  134,
/* GC.GG..GU */
    89,  242,  149,  220,
   242,  242,  242,  242,
    58,  242,  118,  114,
   228,  242,  213,  115,
/* GC.GU..GU */
   242,  242,  242,  242,
    87,  242,  100,   96,
   220,  242,  205,  107,
   120,  242,  105,  129,
/* GC.UA..GU */
   242,  162,  189,  123,
   242,  229,  209,  143,
   242,  154,  209,  143,
   242,  242,  242,  242,
/* GC.UC..GU */
   242,  161,  141,   75,
   242,  146,  173,   60,
   242,  242,  242,  242,
   242,  164,  191,   78,
/* GC.UG..GU */
   242,  222,  277,  211,
   242,  242,  242,  242,
   242,  144,  171,  105,
   242,  239,  172,  228,
/* GC.UU..GU */
   242,  242,  242,  242,
   242,  126,  153,   40,
   242,  231,  164,  220,
   242,   84,  186,   -2,
/* GC.AA..UG */
   224,  237,  149,  242,
   201,  187,  126,  242,
   107,  120,   32,  242,
   242,  242,  242,  242,
/* GC.AC..UG */
   176,  162,  101,  242,
   184,  197,  156,  242,
   242,  242,  242,  242,
   190,  203,  162,  242,
/* GC.AG..UG */
   183,  196,  108,  242,
   242,  242,  242,  242,
   194,  207,  119,  242,
   254,  220,  254,  242,
/* GC.AU..UG */
   242,  242,  242,  242,
   212,  225,  184,  242,
   271,  237,  271,  242,
   136,  102,  136,  242,
/* GC.CA..UG */
   234,  213,  242,  189,
   184,  190,  242,  166,
   117,  143,  242,  119,
   242,  242,  242,  242,
/* GC.CC..UG */
   159,  165,  242,  141,
   194,  173,  242,  149,
   242,  242,  242,  242,
   200,  179,  242,  155,
/* GC.CG..UG */
   193,  219,  242,  195,
   242,  242,  242,  242,
   204,  183,  242,  159,
   217,  243,  242,  219,
/* GC.CU..UG */
   242,  242,  242,  242,
   222,  201,  242,  177,
   234,  260,  242,  236,
    99,   78,  242,   54,
/* GC.GA..UG */
   128,  242,  188,  184,
   105,  242,  165,  114,
    11,  242,   71,  142,
   242,  242,  242,  242,
/* GC.GC..UG */
    80,  242,  140,   89,
   135,  242,  148,  144,
   242,  242,  242,  242,
   141,  242,  154,  150,
/* GC.GG..UG */
    87,  242,  147,  218,
   242,  242,  242,  242,
    98,  242,  158,  154,
   233,  242,  218,  120,
/* GC.GU..UG */
   242,  242,  242,  242,
   163,  242,  176,  172,
   250,  242,  235,  137,
   115,  242,  100,  124,
/* GC.UA..UG */
   242,  214,  241,  175,
   242,  191,  171,  105,
   242,  144,  199,  133,
   242,  242,  242,  242,
/* GC.UC..UG */
   242,  166,  146,   80,
   242,  174,  201,   88,
   242,  242,  242,  242,
   242,  180,  207,   94,
/* GC.UG..UG */
   242,  220,  275,  209,
   242,  242,  242,  242,
   242,  184,  211,  145,
   242,  244,  177,  233,
/* GC.UU..UG */
   242,  242,  242,  242,
   242,  202,  229,  116,
   242,  261,  194,  250,
   242,   79,  181,   -7,
/* GC.AA..AU */
   172,  185,   97,  242,
   239,  225,  164,  242,
   117,  130,   42,  242,
   242,  242,  242,  242,
/* GC.AC..AU */
   171,  157,   96,  242,
   156,  169,  128,  242,
   242,  242,  242,  242,
   174,  187,  146,  242,
/* GC.AG..AU */
   185,  198,  110,  242,
   242,  242,  242,  242,
   154,  167,   79,  242,
   249,  215,  249,  242,
/* GC.AU..AU */
   242,  242,  242,  242,
   136,  149,  108,  242,
   241,  207,  241,  242,
   141,  107,  141,  242,
/* GC.CA..AU */
   182,  161,  242,  137,
   222,  228,  242,  204,
   127,  153,  242,  129,
   242,  242,  242,  242,
/* GC.CC..AU */
   154,  160,  242,  136,
   166,  145,  242,  121,
   242,  242,  242,  242,
   184,  163,  242,  139,
/* GC.CG..AU */
   195,  221,  242,  197,
   242,  242,  242,  242,
   164,  143,  242,  119,
   212,  238,  242,  214,
/* GC.CU..AU */
   242,  242,  242,  242,
   146,  125,  242,  101,
   204,  230,  242,  206,
   104,   83,  242,   59,
/* GC.GA..AU */
    76,  242,  136,  132,
   143,  242,  203,  152,
    21,  242,   81,  152,
   242,  242,  242,  242,
/* GC.GC..AU */
    75,  242,  135,   84,
   107,  242,  120,  116,
   242,  242,  242,  242,
   125,  242,  138,  134,
/* GC.GG..AU */
    89,  242,  149,  220,
   242,  242,  242,  242,
    58,  242,  118,  114,
   228,  242,  213,  115,
/* GC.GU..AU */
   242,  242,  242,  242,
    87,  242,  100,   96,
   220,  242,  205,  107,
   120,  242,  105,  129,
/* GC.UA..AU */
   242,  162,  189,  123,
   242,  229,  209,  143,
   242,  154,  209,  143,
   242,  242,  242,  242,
/* GC.UC..AU */
   242,  161,  141,   75,
   242,  146,  173,   60,
   242,  242,  242,  242,
   242,  164,  191,   78,
/* GC.UG..AU */
   242,  222,  277,  211,
   242,  242,  242,  242,
   242,  144,  171,  105,
   242,  239,  172,  228,
/* GC.UU..AU */
   242,  242,  242,  242,
   242,  126,  153,   40,
   242,  231,  164,  220,
   242,   84,  186,   -2,
/* GC.AA..UA */
   224,  237,  149,  242,
   201,  187,  126,  242,
   107,  120,   32,  242,
   242,  242,  242,  242,
/* GC.AC..UA */
   176,  162,  101,  242,
   184,  197,  156,  242,
   242,  242,  242,  242,
   190,  203,  162,  242,
/* GC.AG..UA */
   183,  196,  108,  242,
   242,  242,  242,  242,
   194,  207,  119,  242,
   254,  220,  254,  242,
/* GC.AU..UA */
   242,  242,  242,  242,
   212,  225,  184,  242,
   271,  237,  271,  242,
   136,  102,  136,  242,
/* GC.CA..UA */
   234,  213,  242,  189,
   184,  190,  242,  166,
   117,  143,  242,  119,
   242,  242,  242,  242,
/* GC.CC..UA */
   159,  165,  242,  141,
   194,  173,  242,  149,
   242,  242,  242,  242,
   200,  179,  242,  155,
/* GC.CG..UA */
   193,  219,  242,  195,
   242,  242,  242,  242,
   204,  183,  242,  159,
   217,  243,  242,  219,
/* GC.CU..UA */
   242,  242,  242,  242,
   222,  201,  242,  177,
   234,  260,  242,  236,
    99,   78,  242,   54,
/* GC.GA..UA */
   128,  242,  188,  184,
   105,  242,  165,  114,
    11,  242,   71,  142,
   242,  242,  242,  242,
/* GC.GC..UA */
    80,  242,  140,   89,
   135,  242,  148,  144,
   242,  242,  242,  242,
   141,  242,  154,  150,
/* GC.GG..UA */
    87,  242,  147,  218,
   242,  242,  242,  242,
    98,  242,  158,  154,
   233,  242,  218,  120,
/* GC.GU..UA */
   242,  242,  242,  242,
   163,  242,  176,  172,
   250,  242,  235,  137,
   115,  242,  100,  124,
/* GC.UA..UA */
   242,  214,  241,  175,
   242,  191,  171,  105,
   242,  144,  199,  133,
   242,  242,  242,  242,
/* GC.UC..UA */
   242,  166,  146,   80,
   242,  174,  201,   88,
   242,  242,  242,  242,
   242,  180,  207,   94,
/* GC.UG..UA */
   242,  220,  275,  209,
   242,  242,  242,  242,
   242,  184,  211,  145,
   242,  244,  177,  233,
/* GC.UU..UA */
   242,  242,  242,  242,
   242,  202,  229,  116,
   242,  261,  194,  250,
   242,   79,  181,   -7,
/* GC.AA.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/* GC.AC.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/* GC.AG.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/* GC.AU.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/* GC.CA.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/* GC.CC.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/* GC.CG.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/* GC.CU.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/* GC.GA.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/* GC.GC.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/* GC.GG.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/* GC.GU.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/* GC.UA.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/* GC.UC.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/* GC.UG.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/* GC.UU.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/* GU.AA..CG */
   150,  149,  163,  242,
   207,  179,  220,  242,
    94,   93,  107,  242,
   242,  242,  242,  242,
/* GU.AC..CG */
   189,  161,  202,  242,
   176,  175,  236,  242,
   242,  242,  242,  242,
   149,  148,  209,  242,
/* GU.AG..CG */
   101,  100,  114,  242,
   242,  242,  242,  242,
   162,  161,  175,  242,
   158,  110,  246,  242,
/* GU.AU..CG */
   242,  242,  242,  242,
   176,  175,  236,  242,
   220,  172,  308,  242,
   130,   82,  218,  242,
/* GU.CA..CG */
   217,  134,  242,  114,
   247,  191,  242,  171,
   161,  125,  242,  105,
   242,  242,  242,  242,
/* GU.CC..CG */
   229,  173,  242,  153,
   243,  160,  242,  140,
   242,  242,  242,  242,
   216,  133,  242,  113,
/* GU.CG..CG */
   168,  132,  242,  112,
   242,  242,  242,  242,
   229,  146,  242,  126,
   178,  142,  242,  122,
/* GU.CU..CG */
   242,  242,  242,  242,
   243,  160,  242,  140,
   240,  204,  242,  184,
   150,   67,  242,   47,
/* GU.GA..CG */
    95,  242,  132,  219,
   152,  242,  189,  229,
    39,  242,   76,  238,
   242,  242,  242,  242,
/* GU.GC..CG */
   134,  242,  171,  211,
   168,  242,  158,  245,
   242,  242,  242,  242,
   141,  242,  131,  218,
/* GU.GG..CG */
    46,  242,   83,  245,
   242,  242,  242,  242,
   107,  242,  144,  231,
   178,  242,  140,  133,
/* GU.GU..CG */
   242,  242,  242,  242,
   168,  242,  158,  245,
   240,  242,  202,  195,
   150,  242,  112,  227,
/* GU.UA..CG */
   242,  152,  227,  119,
   242,  209,  237,  129,
   242,  143,  246,  138,
   242,  242,  242,  242,
/* GU.UC..CG */
   242,  191,  219,  111,
   242,  178,  253,   98,
   242,  242,  242,  242,
   242,  151,  226,   71,
/* GU.UG..CG */
   242,  150,  253,  145,
   242,  242,  242,  242,
   242,  164,  239,  131,
   242,  160,  141,  155,
/* GU.UU..CG */
   242,  242,  242,  242,
   242,  178,  253,   98,
   242,  222,  203,  217,
   242,   85,  235,    5,
/* GU.AA..GC */
   172,  171,  185,  242,
   182,  154,  195,  242,
    76,   75,   89,  242,
   242,  242,  242,  242,
/* GU.AC..GC */
   185,  157,  198,  242,
   161,  160,  221,  242,
   242,  242,  242,  242,
   162,  161,  222,  242,
/* GU.AG..GC */
    97,   96,  110,  242,
   242,  242,  242,  242,
   136,  135,  149,  242,
   189,  141,  277,  242,
/* GU.AU..GC */
   242,  242,  242,  242,
   137,  136,  197,  242,
   132,   84,  220,  242,
   123,   75,  211,  242,
/* GU.CA..GC */
   239,  156,  242,  136,
   222,  166,  242,  146,
   143,  107,  242,   87,
   242,  242,  242,  242,
/* GU.CC..GC */
   225,  169,  242,  149,
   228,  145,  242,  125,
   242,  242,  242,  242,
   229,  146,  242,  126,
/* GU.CG..GC */
   164,  128,  242,  108,
   242,  242,  242,  242,
   203,  120,  242,  100,
   209,  173,  242,  153,
/* GU.CU..GC */
   242,  242,  242,  242,
   204,  121,  242,  101,
   152,  116,  242,   96,
   143,   60,  242,   40,
/* GU.GA..GC */
   117,  242,  154,  241,
   127,  242,  164,  204,
    21,  242,   58,  220,
   242,  242,  242,  242,
/* GU.GC..GC */
   130,  242,  167,  207,
   153,  242,  143,  230,
   242,  242,  242,  242,
   154,  242,  144,  231,
/* GU.GG..GC */
    42,  242,   79,  241,
   242,  242,  242,  242,
    81,  242,  118,  205,
   209,  242,  171,  164,
/* GU.GU..GC */
   242,  242,  242,  242,
   129,  242,  119,  206,
   152,  242,  114,  107,
   143,  242,  105,  220,
/* GU.UA..GC */
   242,  174,  249,  141,
   242,  184,  212,  104,
   242,  125,  228,  120,
   242,  242,  242,  242,
/* GU.UC..GC */
   242,  187,  215,  107,
   242,  163,  238,   83,
   242,  242,  242,  242,
   242,  164,  239,   84,
/* GU.UG..GC */
   242,  146,  249,  141,
   242,  242,  242,  242,
   242,  138,  213,  105,
   242,  191,  172,  186,
/* GU.UU..GC */
   242,  242,  242,  242,
   242,  139,  214,   59,
   242,  134,  115,  129,
   242,   78,  228,   -2,
/* GU.AA..GU */
   160,  180,  194,  242,
   248,  220,  261,  242,
   126,  125,  139,  242,
   242,  242,  242,  242,
/* GU.AC..GU */
   180,  105,  193,  242,
   165,  164,  225,  242,
   242,  242,  242,  242,
   183,  182,  243,  242,
/* GU.AG..GU */
   194,  193,  186,  242,
   242,  242,  242,  242,
   163,  162,  176,  242,
   258,  210,  346,  242,
/* GU.AU..GU */
   242,  242,  242,  242,
   145,  144,  205,  242,
   250,  202,  338,  242,
   150,  102,  238,  242,
/* GU.CA..GU */
   248,  165,  242,  145,
   241,  232,  242,  212,
   193,  157,  242,  137,
   242,  242,  242,  242,
/* GU.CC..GU */
   220,  164,  242,  144,
   232,  128,  242,  129,
   242,  242,  242,  242,
   250,  167,  242,  147,
/* GU.CG..GU */
   261,  225,  242,  205,
   242,  242,  242,  242,
   230,  147,  242,  127,
   278,  242,  242,  222,
/* GU.CU..GU */
   242,  242,  242,  242,
   212,  129,  242,   89,
   270,  234,  242,  214,
   170,   87,  242,   67,
/* GU.GA..GU */
   126,  242,  163,  250,
   193,  242,  230,  270,
    51,  242,  108,  270,
   242,  242,  242,  242,
/* GU.GC..GU */
   125,  242,  162,  202,
   157,  242,  147,  234,
   242,  242,  242,  242,
   175,  242,  165,  252,
/* GU.GG..GU */
   139,  242,  176,  338,
   242,  242,  242,  242,
   108,  242,  125,  232,
   278,  242,  240,  233,
/* GU.GU..GU */
   242,  242,  242,  242,
   137,  242,  127,  214,
   270,  242,  232,  204,
   170,  242,  132,  247,
/* GU.UA..GU */
   242,  183,  258,  150,
   242,  250,  278,  170,
   242,  175,  278,  170,
   242,  242,  242,  242,
/* GU.UC..GU */
   242,  182,  210,  102,
   242,  167,  242,   87,
   242,  242,  242,  242,
   242,  164,  260,  105,
/* GU.UG..GU */
   242,  243,  346,  238,
   242,  242,  242,  242,
   242,  165,  240,  132,
   242,  260,  220,  255,
/* GU.UU..GU */
   242,  242,  242,  242,
   242,  147,  222,   67,
   242,  252,  233,  247,
   242,  105,  255,    4,
/* GU.AA..UG */
   233,  232,  246,  242,
   210,  182,  223,  242,
   116,  115,  129,  242,
   242,  242,  242,  242,
/* GU.AC..UG */
   185,  157,  198,  242,
   193,  192,  253,  242,
   242,  242,  242,  242,
   199,  198,  259,  242,
/* GU.AG..UG */
   192,  191,  205,  242,
   242,  242,  242,  242,
   203,  202,  216,  242,
   263,  215,  351,  242,
/* GU.AU..UG */
   242,  242,  242,  242,
   221,  220,  281,  242,
   280,  232,  368,  242,
   145,   97,  233,  242,
/* GU.CA..UG */
   300,  217,  242,  197,
   250,  194,  242,  174,
   183,  147,  242,  127,
   242,  242,  242,  242,
/* GU.CC..UG */
   225,  169,  242,  149,
   260,  177,  242,  157,
   242,  242,  242,  242,
   266,  183,  242,  163,
/* GU.CG..UG */
   259,  223,  242,  203,
   242,  242,  242,  242,
   270,  187,  242,  167,
   283,  247,  242,  227,
/* GU.CU..UG */
   242,  242,  242,  242,
   288,  205,  242,  185,
   300,  264,  242,  244,
   165,   82,  242,   62,
/* GU.GA..UG */
   178,  242,  215,  302,
   155,  242,  192,  232,
    61,  242,   98,  260,
   242,  242,  242,  242,
/* GU.GC..UG */
   130,  242,  167,  207,
   185,  242,  175,  262,
   242,  242,  242,  242,
   191,  242,  181,  268,
/* GU.GG..UG */
   137,  242,  174,  336,
   242,  242,  242,  242,
   148,  242,  185,  272,
   283,  242,  245,  238,
/* GU.GU..UG */
   242,  242,  242,  242,
   213,  242,  203,  290,
   300,  242,  262,  255,
   165,  242,  127,  242,
/* GU.UA..UG */
   242,  235,  310,  202,
   242,  212,  240,  132,
   242,  165,  268,  160,
   242,  242,  242,  242,
/* GU.UC..UG */
   242,  187,  215,  107,
   242,  195,  270,  115,
   242,  242,  242,  242,
   242,  201,  276,  121,
/* GU.UG..UG */
   242,  241,  344,  236,
   242,  242,  242,  242,
   242,  205,  280,  172,
   242,  265,  246,  260,
/* GU.UU..UG */
   242,  242,  242,  242,
   242,  223,  298,  143,
   242,  282,  263,  277,
   242,  100,  250,   20,
/* GU.AA..AU */
   160,  180,  194,  242,
   248,  220,  261,  242,
   126,  125,  139,  242,
   242,  242,  242,  242,
/* GU.AC..AU */
   180,  105,  193,  242,
   165,  164,  225,  242,
   242,  242,  242,  242,
   183,  182,  243,  242,
/* GU.AG..AU */
   194,  193,  186,  242,
   242,  242,  242,  242,
   163,  162,  176,  242,
   258,  210,  346,  242,
/* GU.AU..AU */
   242,  242,  242,  242,
   145,  144,  205,  242,
   250,  202,  338,  242,
   150,  102,  238,  242,
/* GU.CA..AU */
   248,  165,  242,  145,
   241,  232,  242,  212,
   193,  157,  242,  137,
   242,  242,  242,  242,
/* GU.CC..AU */
   220,  164,  242,  144,
   232,  128,  242,  129,
   242,  242,  242,  242,
   250,  167,  242,  147,
/* GU.CG..AU */
   261,  225,  242,  205,
   242,  242,  242,  242,
   230,  147,  242,  127,
   278,  242,  242,  222,
/* GU.CU..AU */
   242,  242,  242,  242,
   212,  129,  242,   89,
   270,  234,  242,  214,
   170,   87,  242,   67,
/* GU.GA..AU */
   126,  242,  163,  250,
   193,  242,  230,  270,
    51,  242,  108,  270,
   242,  242,  242,  242,
/* GU.GC..AU */
   125,  242,  162,  202,
   157,  242,  147,  234,
   242,  242,  242,  242,
   175,  242,  165,  252,
/* GU.GG..AU */
   139,  242,  176,  338,
   242,  242,  242,  242,
   108,  242,  125,  232,
   278,  242,  240,  233,
/* GU.GU..AU */
   242,  242,  242,  242,
   137,  242,  127,  214,
   270,  242,  232,  204,
   170,  242,  132,  247,
/* GU.UA..AU */
   242,  183,  258,  150,
   242,  250,  278,  170,
   242,  175,  278,  170,
   242,  242,  242,  242,
/* GU.UC..AU */
   242,  182,  210,  102,
   242,  167,  242,   87,
   242,  242,  242,  242,
   242,  164,  260,  105,
/* GU.UG..AU */
   242,  243,  346,  238,
   242,  242,  242,  242,
   242,  165,  240,  132,
   242,  260,  220,  255,
/* GU.UU..AU */
   242,  242,  242,  242,
   242,  147,  222,   67,
   242,  252,  233,  247,
   242,  105,  255,    4,
/* GU.AA..UA */
   233,  232,  246,  242,
   210,  182,  223,  242,
   116,  115,  129,  242,
   242,  242,  242,  242,
/* GU.AC..UA */
   185,  157,  198,  242,
   193,  192,  253,  242,
   242,  242,  242,  242,
   199,  198,  259,  242,
/* GU.AG..UA */
   192,  191,  205,  242,
   242,  242,  242,  242,
   203,  202,  216,  242,
   263,  215,  351,  242,
/* GU.AU..UA */
   242,  242,  242,  242,
   221,  220,  281,  242,
   280,  232,  368,  242,
   145,   97,  233,  242,
/* GU.CA..UA */
   300,  217,  242,  197,
   250,  194,  242,  174,
   183,  147,  242,  127,
   242,  242,  242,  242,
/* GU.CC..UA */
   225,  169,  242,  149,
   260,  177,  242,  157,
   242,  242,  242,  242,
   266,  183,  242,  163,
/* GU.CG..UA */
   259,  223,  242,  203,
   242,  242,  242,  242,
   270,  187,  242,  167,
   283,  247,  242,  227,
/* GU.CU..UA */
   242,  242,  242,  242,
   288,  205,  242,  185,
   300,  264,  242,  244,
   165,   82,  242,   62,
/* GU.GA..UA */
   178,  242,  215,  302,
   155,  242,  192,  232,
    61,  242,   98,  260,
   242,  242,  242,  242,
/* GU.GC..UA */
   130,  242,  167,  207,
   185,  242,  175,  262,
   242,  242,  242,  242,
   191,  242,  181,  268,
/* GU.GG..UA */
   137,  242,  174,  336,
   242,  242,  242,  242,
   148,  242,  185,  272,
   283,  242,  245,  238,
/* GU.GU..UA */
   242,  242,  242,  242,
   213,  242,  203,  290,
   300,  242,  262,  255,
   165,  242,  127,  242,
/* GU.UA..UA */
   242,  235,  310,  202,
   242,  212,  240,  132,
   242,  165,  268,  160,
   242,  242,  242,  242,
/* GU.UC..UA */
   242,  187,  215,  107,
   242,  195,  270,  115,
   242,  242,  242,  242,
   242,  201,  276,  121,
/* GU.UG..UA */
   242,  241,  344,  236,
   242,  242,  242,  242,
   242,  205,  280,  172,
   242,  265,  246,  260,
/* GU.UU..UA */
   242,  242,  242,  242,
   242,  223,  298,  143,
   242,  282,  263,  277,
   242,  100,  250,   20,
/* GU.AA.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/* GU.AC.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/* GU.AG.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/* GU.AU.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/* GU.CA.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/* GU.CC.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/* GU.CG.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/* GU.CU.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/* GU.GA.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/* GU.GC.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/* GU.GG.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/* GU.GU.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/* GU.UA.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/* GU.UC.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/* GU.UG.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/* GU.UU.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/* UG.AA..CG */
   202,  154,  161,  242,
   259,  184,  218,  242,
   146,   98,  105,  242,
   242,  242,  242,  242,
/* UG.AC..CG */
   241,  166,  200,  242,
   228,  180,  234,  242,
   242,  242,  242,  242,
   201,  153,  207,  242,
/* UG.AG..CG */
   153,  105,  112,  242,
   242,  242,  242,  242,
   214,  166,  173,  242,
   210,  115,  244,  242,
/* UG.AU..CG */
   242,  242,  242,  242,
   228,  180,  234,  242,
   272,  177,  306,  242,
   182,   87,  216,  242,
/* UG.CA..CG */
   179,  162,  242,  190,
   209,  219,  242,  247,
   123,  153,  242,  181,
   242,  242,  242,  242,
/* UG.CC..CG */
   191,  201,  242,  229,
   205,  188,  242,  216,
   242,  242,  242,  242,
   178,  161,  242,  189,
/* UG.CG..CG */
   130,  160,  242,  188,
   242,  242,  242,  242,
   191,  174,  242,  202,
   140,  170,  242,  198,
/* UG.CU..CG */
   242,  242,  242,  242,
   205,  188,  242,  216,
   202,  232,  242,  260,
   112,   95,  242,  123,
/* UG.GA..CG */
    85,  242,  172,  249,
   142,  242,  229,  259,
    29,  242,  116,  268,
   242,  242,  242,  242,
/* UG.GC..CG */
   124,  242,  211,  241,
   158,  242,  198,  275,
   242,  242,  242,  242,
   131,  242,  171,  248,
/* UG.GG..CG */
    36,  242,  123,  275,
   242,  242,  242,  242,
    97,  242,  184,  261,
   168,  242,  180,  163,
/* UG.GU..CG */
   242,  242,  242,  242,
   158,  242,  198,  275,
   230,  242,  242,  225,
   140,  242,  152,  257,
/* UG.UA..CG */
   242,  168,  232,  114,
   242,  225,  242,  124,
   242,  159,  251,  133,
   242,  242,  242,  242,
/* UG.UC..CG */
   242,  207,  224,  106,
   242,  194,  258,   93,
   242,  242,  242,  242,
   242,  167,  231,   66,
/* UG.UG..CG */
   242,  166,  258,  140,
   242,  242,  242,  242,
   242,  180,  244,  126,
   242,  176,  146,  150,
/* UG.UU..CG */
   242,  242,  242,  242,
   242,  194,  258,   93,
   242,  238,  208,  212,
   242,  101,  240,    0,
/* UG.AA..GC */
   224,  176,  183,  242,
   234,  159,  193,  242,
   128,   80,   87,  242,
   242,  242,  242,  242,
/* UG.AC..GC */
   237,  162,  196,  242,
   213,  165,  219,  242,
   242,  242,  242,  242,
   214,  166,  220,  242,
/* UG.AG..GC */
   149,  101,  108,  242,
   242,  242,  242,  242,
   188,  140,  147,  242,
   241,  146,  275,  242,
/* UG.AU..GC */
   242,  242,  242,  242,
   189,  141,  195,  242,
   184,   89,  218,  242,
   175,   80,  209,  242,
/* UG.CA..GC */
   201,  184,  242,  212,
   184,  194,  242,  222,
   105,  135,  242,  163,
   242,  242,  242,  242,
/* UG.CC..GC */
   187,  197,  242,  225,
   190,  173,  242,  201,
   242,  242,  242,  242,
   191,  174,  242,  202,
/* UG.CG..GC */
   126,  156,  242,  184,
   242,  242,  242,  242,
   165,  148,  242,  176,
   171,  201,  242,  229,
/* UG.CU..GC */
   242,  242,  242,  242,
   166,  149,  242,  177,
   114,  144,  242,  172,
   105,   88,  242,  116,
/* UG.GA..GC */
   107,  242,  194,  271,
   117,  242,  204,  234,
    11,  242,   98,  250,
   242,  242,  242,  242,
/* UG.GC..GC */
   120,  242,  207,  237,
   143,  242,  183,  260,
   242,  242,  242,  242,
   144,  242,  184,  261,
/* UG.GG..GC */
    32,  242,  119,  271,
   242,  242,  242,  242,
    71,  242,  158,  235,
   199,  242,  211,  194,
/* UG.GU..GC */
   242,  242,  242,  242,
   119,  242,  159,  236,
   142,  242,  154,  137,
   133,  242,  145,  250,
/* UG.UA..GC */
   242,  190,  254,  136,
   242,  200,  217,   99,
   242,  141,  233,  115,
   242,  242,  242,  242,
/* UG.UC..GC */
   242,  203,  220,  102,
   242,  179,  243,   78,
   242,  242,  242,  242,
   242,  180,  244,   79,
/* UG.UG..GC */
   242,  162,  254,  136,
   242,  242,  242,  242,
   242,  154,  218,  100,
   242,  207,  177,  181,
/* UG.UU..GC */
   242,  242,  242,  242,
   242,  155,  219,   54,
   242,  150,  120,  124,
   242,   94,  233,   -7,
/* UG.AA..GU */
   233,  185,  192,  242,
   300,  225,  259,  242,
   178,  130,  137,  242,
   242,  242,  242,  242,
/* UG.AC..GU */
   232,  157,  191,  242,
   217,  169,  223,  242,
   242,  242,  242,  242,
   235,  187,  241,  242,
/* UG.AG..GU */
   246,  198,  205,  242,
   242,  242,  242,  242,
   215,  167,  174,  242,
   310,  215,  344,  242,
/* UG.AU..GU */
   242,  242,  242,  242,
   197,  149,  203,  242,
   302,  207,  336,  242,
   202,  107,  236,  242,
/* UG.CA..GU */
   210,  193,  242,  221,
   250,  260,  242,  288,
   155,  185,  242,  213,
   242,  242,  242,  242,
/* UG.CC..GU */
   182,  192,  242,  220,
   194,  177,  242,  205,
   242,  242,  242,  242,
   212,  195,  242,  223,
/* UG.CG..GU */
   223,  253,  242,  281,
   242,  242,  242,  242,
   192,  175,  242,  203,
   240,  270,  242,  298,
/* UG.CU..GU */
   242,  242,  242,  242,
   174,  157,  242,  185,
   232,  262,  242,  290,
   132,  115,  242,  143,
/* UG.GA..GU */
   116,  242,  203,  280,
   183,  242,  270,  300,
    61,  242,  148,  300,
   242,  242,  242,  242,
/* UG.GC..GU */
   115,  242,  202,  232,
   147,  242,  187,  264,
   242,  242,  242,  242,
   165,  242,  205,  282,
/* UG.GG..GU */
   129,  242,  216,  368,
   242,  242,  242,  242,
    98,  242,  185,  262,
   268,  242,  280,  263,
/* UG.GU..GU */
   242,  242,  242,  242,
   127,  242,  167,  244,
   260,  242,  272,  255,
   160,  242,  172,  277,
/* UG.UA..GU */
   242,  199,  263,  145,
   242,  266,  283,  165,
   242,  191,  283,  165,
   242,  242,  242,  242,
/* UG.UC..GU */
   242,  198,  215,   97,
   242,  183,  247,   82,
   242,  242,  242,  242,
   242,  201,  265,  100,
/* UG.UG..GU */
   242,  259,  351,  233,
   242,  242,  242,  242,
   242,  181,  245,  127,
   242,  276,  246,  250,
/* UG.UU..GU */
   242,  242,  242,  242,
   242,  163,  227,   62,
   242,  268,  238,  242,
   242,  121,  260,   20,
/* UG.AA..UG */
   264,  237,  244,  242,
   262,  187,  221,  242,
   168,  120,  127,  242,
   242,  242,  242,  242,
/* UG.AC..UG */
   237,  114,  196,  242,
   245,  197,  251,  242,
   242,  242,  242,  242,
   251,  203,  257,  242,
/* UG.AG..UG */
   244,  196,  182,  242,
   242,  242,  242,  242,
   255,  207,  214,  242,
   315,  220,  349,  242,
/* UG.AU..UG */
   242,  242,  242,  242,
   273,  225,  279,  242,
   332,  237,  366,  242,
   197,  102,  231,  242,
/* UG.CA..UG */
   262,  245,  242,  273,
   164,  222,  242,  250,
   145,  175,  242,  203,
   242,  242,  242,  242,
/* UG.CC..UG */
   187,  197,  242,  225,
   222,  185,  242,  233,
   242,  242,  242,  242,
   228,  211,  242,  239,
/* UG.CG..UG */
   221,  251,  242,  279,
   242,  242,  242,  242,
   232,  215,  242,  243,
   245,  275,  242,  303,
/* UG.CU..UG */
   242,  242,  242,  242,
   250,  233,  242,  240,
   262,  292,  242,  320,
   127,  110,  242,  138,
/* UG.GA..UG */
   168,  242,  255,  332,
   145,  242,  232,  262,
    30,  242,  138,  290,
   242,  242,  242,  242,
/* UG.GC..UG */
   120,  242,  207,  237,
   175,  242,  215,  292,
   242,  242,  242,  242,
   181,  242,  221,  298,
/* UG.GG..UG */
   127,  242,  214,  366,
   242,  242,  242,  242,
   138,  242,  204,  302,
   273,  242,  285,  268,
/* UG.GU..UG */
   242,  242,  242,  242,
   203,  242,  243,  320,
   290,  242,  302,  265,
   155,  242,  167,  272,
/* UG.UA..UG */
   242,  251,  315,  197,
   242,  228,  245,  127,
   242,  181,  273,  155,
   242,  242,  242,  242,
/* UG.UC..UG */
   242,  203,  220,  102,
   242,  211,  275,  110,
   242,  242,  242,  242,
   242,  196,  281,  116,
/* UG.UG..UG */
   242,  257,  349,  231,
   242,  242,  242,  242,
   242,  221,  285,  167,
   242,  281,  231,  255,
/* UG.UU..UG */
   242,  242,  242,  242,
   242,  239,  303,  138,
   242,  298,  268,  272,
   242,  116,  255,   -7,
/* UG.AA..AU */
   233,  185,  192,  242,
   300,  225,  259,  242,
   178,  130,  137,  242,
   242,  242,  242,  242,
/* UG.AC..AU */
   232,  157,  191,  242,
   217,  169,  223,  242,
   242,  242,  242,  242,
   235,  187,  241,  242,
/* UG.AG..AU */
   246,  198,  205,  242,
   242,  242,  242,  242,
   215,  167,  174,  242,
   310,  215,  344,  242,
/* UG.AU..AU */
   242,  242,  242,  242,
   197,  149,  203,  242,
   302,  207,  336,  242,
   202,  107,  236,  242,
/* UG.CA..AU */
   210,  193,  242,  221,
   250,  260,  242,  288,
   155,  185,  242,  213,
   242,  242,  242,  242,
/* UG.CC..AU */
   182,  192,  242,  220,
   194,  177,  242,  205,
   242,  242,  242,  242,
   212,  195,  242,  223,
/* UG.CG..AU */
   223,  253,  242,  281,
   242,  242,  242,  242,
   192,  175,  242,  203,
   240,  270,  242,  298,
/* UG.CU..AU */
   242,  242,  242,  242,
   174,  157,  242,  185,
   232,  262,  242,  290,
   132,  115,  242,  143,
/* UG.GA..AU */
   116,  242,  203,  280,
   183,  242,  270,  300,
    61,  242,  148,  300,
   242,  242,  242,  242,
/* UG.GC..AU */
   115,  242,  202,  232,
   147,  242,  187,  264,
   242,  242,  242,  242,
   165,  242,  205,  282,
/* UG.GG..AU */
   129,  242,  216,  368,
   242,  242,  242,  242,
    98,  242,  185,  262,
   268,  242,  280,  263,
/* UG.GU..AU */
   242,  242,  242,  242,
   127,  242,  167,  244,
   260,  242,  272,  255,
   160,  242,  172,  277,
/* UG.UA..AU */
   242,  199,  263,  145,
   242,  266,  283,  165,
   242,  191,  283,  165,
   242,  242,  242,  242,
/* UG.UC..AU */
   242,  198,  215,   97,
   242,  183,  247,   82,
   242,  242,  242,  242,
   242,  201,  265,  100,
/* UG.UG..AU */
   242,  259,  351,  233,
   242,  242,  242,  242,
   242,  181,  245,  127,
   242,  276,  246,  250,
/* UG.UU..AU */
   242,  242,  242,  242,
   242,  163,  227,   62,
   242,  268,  238,  242,
   242,  121,  260,   20,
/* UG.AA..UA */
   264,  237,  244,  242,
   262,  187,  221,  242,
   168,  120,  127,  242,
   242,  242,  242,  242,
/* UG.AC..UA */
   237,  114,  196,  242,
   245,  197,  251,  242,
   242,  242,  242,  242,
   251,  203,  257,  242,
/* UG.AG..UA */
   244,  196,  182,  242,
   242,  242,  242,  242,
   255,  207,  214,  242,
   315,  220,  349,  242,
/* UG.AU..UA */
   242,  242,  242,  242,
   273,  225,  279,  242,
   332,  237,  366,  242,
   197,  102,  231,  242,
/* UG.CA..UA */
   262,  245,  242,  273,
   164,  222,  242,  250,
   145,  175,  242,  203,
   242,  242,  242,  242,
/* UG.CC..UA */
   187,  197,  242,  225,
   222,  185,  242,  233,
   242,  242,  242,  242,
   228,  211,  242,  239,
/* UG.CG..UA */
   221,  251,  242,  279,
   242,  242,  242,  242,
   232,  215,  242,  243,
   245,  275,  242,  303,
/* UG.CU..UA */
   242,  242,  242,  242,
   250,  233,  242,  240,
   262,  292,  242,  320,
   127,  110,  242,  138,
/* UG.GA..UA */
   168,  242,  255,  332,
   145,  242,  232,  262,
    30,  242,  138,  290,
   242,  242,  242,  242,
/* UG.GC..UA */
   120,  242,  207,  237,
   175,  242,  215,  292,
   242,  242,  242,  242,
   181,  242,  221,  298,
/* UG.GG..UA */
   127,  242,  214,  366,
   242,  242,  242,  242,
   138,  242,  204,  302,
   273,  242,  285,  268,
/* UG.GU..UA */
   242,  242,  242,  242,
   203,  242,  243,  320,
   290,  242,  302,  265,
   155,  242,  167,  272,
/* UG.UA..UA */
   242,  251,  315,  197,
   242,  228,  245,  127,
   242,  181,  273,  155,
   242,  242,  242,  242,
/* UG.UC..UA */
   242,  203,  220,  102,
   242,  211,  275,  110,
   242,  242,  242,  242,
   242,  196,  281,  116,
/* UG.UG..UA */
   242,  257,  349,  231,
   242,  242,  242,  242,
   242,  221,  285,  167,
   242,  281,  231,  255,
/* UG.UU..UA */
   242,  242,  242,  242,
   242,  239,  303,  138,
   242,  298,  268,  272,
   242,  116,  255,   -7,
/* UG.AA.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/* UG.AC.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/* UG.AG.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/* UG.AU.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/* UG.CA.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/* UG.CC.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/* UG.CG.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/* UG.CU.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/* UG.GA.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/* UG.GC.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/* UG.GG.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/* UG.GU.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/* UG.UA.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/* UG.UC.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/* UG.UG.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/* UG.UU.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/* AU.AA..CG */
   150,  149,  163,  242,
   207,  179,  220,  242,
    94,   93,  107,  242,
   242,  242,  242,  242,
/* AU.AC..CG */
   189,  161,  202,  242,
   176,  175,  236,  242,
   242,  242,  242,  242,
   149,  148,  209,  242,
/* AU.AG..CG */
   101,  100,  114,  242,
   242,  242,  242,  242,
   162,  161,  175,  242,
   158,  110,  246,  242,
/* AU.AU..CG */
   242,  242,  242,  242,
   176,  175,  236,  242,
   220,  172,  308,  242,
   130,   82,  218,  242,
/* AU.CA..CG */
   217,  134,  242,  114,
   247,  191,  242,  171,
   161,  125,  242,  105,
   242,  242,  242,  242,
/* AU.CC..CG */
   229,  173,  242,  153,
   243,  160,  242,  140,
   242,  242,  242,  242,
   216,  133,  242,  113,
/* AU.CG..CG */
   168,  132,  242,  112,
   242,  242,  242,  242,
   229,  146,  242,  126,
   178,  142,  242,  122,
/* AU.CU..CG */
   242,  242,  242,  242,
   243,  160,  242,  140,
   240,  204,  242,  184,
   150,   67,  242,   47,
/* AU.GA..CG */
    95,  242,  132,  219,
   152,  242,  189,  229,
    39,  242,   76,  238,
   242,  242,  242,  242,
/* AU.GC..CG */
   134,  242,  171,  211,
   168,  242,  158,  245,
   242,  242,  242,  242,
   141,  242,  131,  218,
/* AU.GG..CG */
    46,  242,   83,  245,
   242,  242,  242,  242,
   107,  242,  144,  231,
   178,  242,  140,  133,
/* AU.GU..CG */
   242,  242,  242,  242,
   168,  242,  158,  245,
   240,  242,  202,  195,
   150,  242,  112,  227,
/* AU.UA..CG */
   242,  152,  227,  119,
   242,  209,  237,  129,
   242,  143,  246,  138,
   242,  242,  242,  242,
/* AU.UC..CG */
   242,  191,  219,  111,
   242,  178,  253,   98,
   242,  242,  242,  242,
   242,  151,  226,   71,
/* AU.UG..CG */
   242,  150,  253,  145,
   242,  242,  242,  242,
   242,  164,  239,  131,
   242,  160,  141,  155,
/* AU.UU..CG */
   242,  242,  242,  242,
   242,  178,  253,   98,
   242,  222,  203,  217,
   242,   85,  235,    5,
/* AU.AA..GC */
   172,  171,  185,  242,
   182,  154,  195,  242,
    76,   75,   89,  242,
   242,  242,  242,  242,
/* AU.AC..GC */
   185,  157,  198,  242,
   161,  160,  221,  242,
   242,  242,  242,  242,
   162,  161,  222,  242,
/* AU.AG..GC */
    97,   96,  110,  242,
   242,  242,  242,  242,
   136,  135,  149,  242,
   189,  141,  277,  242,
/* AU.AU..GC */
   242,  242,  242,  242,
   137,  136,  197,  242,
   132,   84,  220,  242,
   123,   75,  211,  242,
/* AU.CA..GC */
   239,  156,  242,  136,
   222,  166,  242,  146,
   143,  107,  242,   87,
   242,  242,  242,  242,
/* AU.CC..GC */
   225,  169,  242,  149,
   228,  145,  242,  125,
   242,  242,  242,  242,
   229,  146,  242,  126,
/* AU.CG..GC */
   164,  128,  242,  108,
   242,  242,  242,  242,
   203,  120,  242,  100,
   209,  173,  242,  153,
/* AU.CU..GC */
   242,  242,  242,  242,
   204,  121,  242,  101,
   152,  116,  242,   96,
   143,   60,  242,   40,
/* AU.GA..GC */
   117,  242,  154,  241,
   127,  242,  164,  204,
    21,  242,   58,  220,
   242,  242,  242,  242,
/* AU.GC..GC */
   130,  242,  167,  207,
   153,  242,  143,  230,
   242,  242,  242,  242,
   154,  242,  144,  231,
/* AU.GG..GC */
    42,  242,   79,  241,
   242,  242,  242,  242,
    81,  242,  118,  205,
   209,  242,  171,  164,
/* AU.GU..GC */
   242,  242,  242,  242,
   129,  242,  119,  206,
   152,  242,  114,  107,
   143,  242,  105,  220,
/* AU.UA..GC */
   242,  174,  249,  141,
   242,  184,  212,  104,
   242,  125,  228,  120,
   242,  242,  242,  242,
/* AU.UC..GC */
   242,  187,  215,  107,
   242,  163,  238,   83,
   242,  242,  242,  242,
   242,  164,  239,   84,
/* AU.UG..GC */
   242,  146,  249,  141,
   242,  242,  242,  242,
   242,  138,  213,  105,
   242,  191,  172,  186,
/* AU.UU..GC */
   242,  242,  242,  242,
   242,  139,  214,   59,
   242,  134,  115,  129,
   242,   78,  228,   -2,
/* AU.AA..GU */
   160,  180,  194,  242,
   248,  220,  261,  242,
   126,  125,  139,  242,
   242,  242,  242,  242,
/* AU.AC..GU */
   180,  105,  193,  242,
   165,  164,  225,  242,
   242,  242,  242,  242,
   183,  182,  243,  242,
/* AU.AG..GU */
   194,  193,  186,  242,
   242,  242,  242,  242,
   163,  162,  176,  242,
   258,  210,  346,  242,
/* AU.AU..GU */
   242,  242,  242,  242,
   145,  144,  205,  242,
   250,  202,  338,  242,
   150,  102,  238,  242,
/* AU.CA..GU */
   248,  165,  242,  145,
   241,  232,  242,  212,
   193,  157,  242,  137,
   242,  242,  242,  242,
/* AU.CC..GU */
   220,  164,  242,  144,
   232,  128,  242,  129,
   242,  242,  242,  242,
   250,  167,  242,  147,
/* AU.CG..GU */
   261,  225,  242,  205,
   242,  242,  242,  242,
   230,  147,  242,  127,
   278,  242,  242,  222,
/* AU.CU..GU */
   242,  242,  242,  242,
   212,  129,  242,   89,
   270,  234,  242,  214,
   170,   87,  242,   67,
/* AU.GA..GU */
   126,  242,  163,  250,
   193,  242,  230,  270,
    51,  242,  108,  270,
   242,  242,  242,  242,
/* AU.GC..GU */
   125,  242,  162,  202,
   157,  242,  147,  234,
   242,  242,  242,  242,
   175,  242,  165,  252,
/* AU.GG..GU */
   139,  242,  176,  338,
   242,  242,  242,  242,
   108,  242,  125,  232,
   278,  242,  240,  233,
/* AU.GU..GU */
   242,  242,  242,  242,
   137,  242,  127,  214,
   270,  242,  232,  204,
   170,  242,  132,  247,
/* AU.UA..GU */
   242,  183,  258,  150,
   242,  250,  278,  170,
   242,  175,  278,  170,
   242,  242,  242,  242,
/* AU.UC..GU */
   242,  182,  210,  102,
   242,  167,  242,   87,
   242,  242,  242,  242,
   242,  164,  260,  105,
/* AU.UG..GU */
   242,  243,  346,  238,
   242,  242,  242,  242,
   242,  165,  240,  132,
   242,  260,  220,  255,
/* AU.UU..GU */
   242,  242,  242,  242,
   242,  147,  222,   67,
   242,  252,  233,  247,
   242,  105,  255,    4,
/* AU.AA..UG */
   233,  232,  246,  242,
   210,  182,  223,  242,
   116,  115,  129,  242,
   242,  242,  242,  242,
/* AU.AC..UG */
   185,  157,  198,  242,
   193,  192,  253,  242,
   242,  242,  242,  242,
   199,  198,  259,  242,
/* AU.AG..UG */
   192,  191,  205,  242,
   242,  242,  242,  242,
   203,  202,  216,  242,
   263,  215,  351,  242,
/* AU.AU..UG */
   242,  242,  242,  242,
   221,  220,  281,  242,
   280,  232,  368,  242,
   145,   97,  233,  242,
/* AU.CA..UG */
   300,  217,  242,  197,
   250,  194,  242,  174,
   183,  147,  242,  127,
   242,  242,  242,  242,
/* AU.CC..UG */
   225,  169,  242,  149,
   260,  177,  242,  157,
   242,  242,  242,  242,
   266,  183,  242,  163,
/* AU.CG..UG */
   259,  223,  242,  203,
   242,  242,  242,  242,
   270,  187,  242,  167,
   283,  247,  242,  227,
/* AU.CU..UG */
   242,  242,  242,  242,
   288,  205,  242,  185,
   300,  264,  242,  244,
   165,   82,  242,   62,
/* AU.GA..UG */
   178,  242,  215,  302,
   155,  242,  192,  232,
    61,  242,   98,  260,
   242,  242,  242,  242,
/* AU.GC..UG */
   130,  242,  167,  207,
   185,  242,  175,  262,
   242,  242,  242,  242,
   191,  242,  181,  268,
/* AU.GG..UG */
   137,  242,  174,  336,
   242,  242,  242,  242,
   148,  242,  185,  272,
   283,  242,  245,  238,
/* AU.GU..UG */
   242,  242,  242,  242,
   213,  242,  203,  290,
   300,  242,  262,  255,
   165,  242,  127,  242,
/* AU.UA..UG */
   242,  235,  310,  202,
   242,  212,  240,  132,
   242,  165,  268,  160,
   242,  242,  242,  242,
/* AU.UC..UG */
   242,  187,  215,  107,
   242,  195,  270,  115,
   242,  242,  242,  242,
   242,  201,  276,  121,
/* AU.UG..UG */
   242,  241,  344,  236,
   242,  242,  242,  242,
   242,  205,  280,  172,
   242,  265,  246,  260,
/* AU.UU..UG */
   242,  242,  242,  242,
   242,  223,  298,  143,
   242,  282,  263,  277,
   242,  100,  250,   20,
/* AU.AA..AU */
   160,  180,  194,  242,
   248,  220,  261,  242,
   126,  125,  139,  242,
   242,  242,  242,  242,
/* AU.AC..AU */
   180,  105,  193,  242,
   165,  164,  225,  242,
   242,  242,  242,  242,
   183,  182,  243,  242,
/* AU.AG..AU */
   194,  193,  186,  242,
   242,  242,  242,  242,
   163,  162,  176,  242,
   258,  210,  346,  242,
/* AU.AU..AU */
   242,  242,  242,  242,
   145,  144,  205,  242,
   250,  202,  338,  242,
   150,  102,  238,  242,
/* AU.CA..AU */
   248,  165,  242,  145,
   241,  232,  242,  212,
   193,  157,  242,  137,
   242,  242,  242,  242,
/* AU.CC..AU */
   220,  164,  242,  144,
   232,  128,  242,  129,
   242,  242,  242,  242,
   250,  167,  242,  147,
/* AU.CG..AU */
   261,  225,  242,  205,
   242,  242,  242,  242,
   230,  147,  242,  127,
   278,  242,  242,  222,
/* AU.CU..AU */
   242,  242,  242,  242,
   212,  129,  242,   89,
   270,  234,  242,  214,
   170,   87,  242,   67,
/* AU.GA..AU */
   126,  242,  163,  250,
   193,  242,  230,  270,
    51,  242,  108,  270,
   242,  242,  242,  242,
/* AU.GC..AU */
   125,  242,  162,  202,
   157,  242,  147,  234,
   242,  242,  242,  242,
   175,  242,  165,  252,
/* AU.GG..AU */
   139,  242,  176,  338,
   242,  242,  242,  242,
   108,  242,  125,  232,
   278,  242,  240,  233,
/* AU.GU..AU */
   242,  242,  242,  242,
   137,  242,  127,  214,
   270,  242,  232,  204,
   170,  242,  132,  247,
/* AU.UA..AU */
   242,  183,  258,  150,
   242,  250,  278,  170,
   242,  175,  278,  170,
   242,  242,  242,  242,
/* AU.UC..AU */
   242,  182,  210,  102,
   242,  167,  242,   87,
   242,  242,  242,  242,
   242,  164,  260,  105,
/* AU.UG..AU */
   242,  243,  346,  238,
   242,  242,  242,  242,
   242,  165,  240,  132,
   242,  260,  220,  255,
/* AU.UU..AU */
   242,  242,  242,  242,
   242,  147,  222,   67,
   242,  252,  233,  247,
   242,  105,  255,    4,
/* AU.AA..UA */
   233,  232,  246,  242,
   210,  182,  223,  242,
   116,  115,  129,  242,
   242,  242,  242,  242,
/* AU.AC..UA */
   185,  157,  198,  242,
   193,  192,  253,  242,
   242,  242,  242,  242,
   199,  198,  259,  242,
/* AU.AG..UA */
   192,  191,  205,  242,
   242,  242,  242,  242,
   203,  202,  216,  242,
   263,  215,  351,  242,
/* AU.AU..UA */
   242,  242,  242,  242,
   221,  220,  281,  242,
   280,  232,  368,  242,
   145,   97,  233,  242,
/* AU.CA..UA */
   300,  217,  242,  197,
   250,  194,  242,  174,
   183,  147,  242,  127,
   242,  242,  242,  242,
/* AU.CC..UA */
   225,  169,  242,  149,
   260,  177,  242,  157,
   242,  242,  242,  242,
   266,  183,  242,  163,
/* AU.CG..UA */
   259,  223,  242,  203,
   242,  242,  242,  242,
   270,  187,  242,  167,
   283,  247,  242,  227,
/* AU.CU..UA */
   242,  242,  242,  242,
   288,  205,  242,  185,
   300,  264,  242,  244,
   165,   82,  242,   62,
/* AU.GA..UA */
   178,  242,  215,  302,
   155,  242,  192,  232,
    61,  242,   98,  260,
   242,  242,  242,  242,
/* AU.GC..UA */
   130,  242,  167,  207,
   185,  242,  175,  262,
   242,  242,  242,  242,
   191,  242,  181,  268,
/* AU.GG..UA */
   137,  242,  174,  336,
   242,  242,  242,  242,
   148,  242,  185,  272,
   283,  242,  245,  238,
/* AU.GU..UA */
   242,  242,  242,  242,
   213,  242,  203,  290,
   300,  242,  262,  255,
   165,  242,  127,  242,
/* AU.UA..UA */
   242,  235,  310,  202,
   242,  212,  240,  132,
   242,  165,  268,  160,
   242,  242,  242,  242,
/* AU.UC..UA */
   242,  187,  215,  107,
   242,  195,  270,  115,
   242,  242,  242,  242,
   242,  201,  276,  121,
/* AU.UG..UA */
   242,  241,  344,  236,
   242,  242,  242,  242,
   242,  205,  280,  172,
   242,  265,  246,  260,
/* AU.UU..UA */
   242,  242,  242,  242,
   242,  223,  298,  143,
   242,  282,  263,  277,
   242,  100,  250,   20,
/* AU.AA.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/* AU.AC.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/* AU.AG.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/* AU.AU.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/* AU.CA.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/* AU.CC.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/* AU.CG.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/* AU.CU.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/* AU.GA.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/* AU.GC.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/* AU.GG.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/* AU.GU.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/* AU.UA.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/* AU.UC.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/* AU.UG.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/* AU.UU.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/* UA.AA..CG */
   202,  154,  161,  242,
   259,  184,  218,  242,
   146,   98,  105,  242,
   242,  242,  242,  242,
/* UA.AC..CG */
   241,  166,  200,  242,
   228,  180,  234,  242,
   242,  242,  242,  242,
   201,  153,  207,  242,
/* UA.AG..CG */
   153,  105,  112,  242,
   242,  242,  242,  242,
   214,  166,  173,  242,
   210,  115,  244,  242,
/* UA.AU..CG */
   242,  242,  242,  242,
   228,  180,  234,  242,
   272,  177,  306,  242,
   182,   87,  216,  242,
/* UA.CA..CG */
   179,  162,  242,  190,
   209,  219,  242,  247,
   123,  153,  242,  181,
   242,  242,  242,  242,
/* UA.CC..CG */
   191,  201,  242,  229,
   205,  188,  242,  216,
   242,  242,  242,  242,
   178,  161,  242,  189,
/* UA.CG..CG */
   130,  160,  242,  188,
   242,  242,  242,  242,
   191,  174,  242,  202,
   140,  170,  242,  198,
/* UA.CU..CG */
   242,  242,  242,  242,
   205,  188,  242,  216,
   202,  232,  242,  260,
   112,   95,  242,  123,
/* UA.GA..CG */
    85,  242,  172,  249,
   142,  242,  229,  259,
    29,  242,  116,  268,
   242,  242,  242,  242,
/* UA.GC..CG */
   124,  242,  211,  241,
   158,  242,  198,  275,
   242,  242,  242,  242,
   131,  242,  171,  248,
/* UA.GG..CG */
    36,  242,  123,  275,
   242,  242,  242,  242,
    97,  242,  184,  261,
   168,  242,  180,  163,
/* UA.GU..CG */
   242,  242,  242,  242,
   158,  242,  198,  275,
   230,  242,  242,  225,
   140,  242,  152,  257,
/* UA.UA..CG */
   242,  168,  232,  114,
   242,  225,  242,  124,
   242,  159,  251,  133,
   242,  242,  242,  242,
/* UA.UC..CG */
   242,  207,  224,  106,
   242,  194,  258,   93,
   242,  242,  242,  242,
   242,  167,  231,   66,
/* UA.UG..CG */
   242,  166,  258,  140,
   242,  242,  242,  242,
   242,  180,  244,  126,
   242,  176,  146,  150,
/* UA.UU..CG */
   242,  242,  242,  242,
   242,  194,  258,   93,
   242,  238,  208,  212,
   242,  101,  240,    0,
/* UA.AA..GC */
   224,  176,  183,  242,
   234,  159,  193,  242,
   128,   80,   87,  242,
   242,  242,  242,  242,
/* UA.AC..GC */
   237,  162,  196,  242,
   213,  165,  219,  242,
   242,  242,  242,  242,
   214,  166,  220,  242,
/* UA.AG..GC */
   149,  101,  108,  242,
   242,  242,  242,  242,
   188,  140,  147,  242,
   241,  146,  275,  242,
/* UA.AU..GC */
   242,  242,  242,  242,
   189,  141,  195,  242,
   184,   89,  218,  242,
   175,   80,  209,  242,
/* UA.CA..GC */
   201,  184,  242,  212,
   184,  194,  242,  222,
   105,  135,  242,  163,
   242,  242,  242,  242,
/* UA.CC..GC */
   187,  197,  242,  225,
   190,  173,  242,  201,
   242,  242,  242,  242,
   191,  174,  242,  202,
/* UA.CG..GC */
   126,  156,  242,  184,
   242,  242,  242,  242,
   165,  148,  242,  176,
   171,  201,  242,  229,
/* UA.CU..GC */
   242,  242,  242,  242,
   166,  149,  242,  177,
   114,  144,  242,  172,
   105,   88,  242,  116,
/* UA.GA..GC */
   107,  242,  194,  271,
   117,  242,  204,  234,
    11,  242,   98,  250,
   242,  242,  242,  242,
/* UA.GC..GC */
   120,  242,  207,  237,
   143,  242,  183,  260,
   242,  242,  242,  242,
   144,  242,  184,  261,
/* UA.GG..GC */
    32,  242,  119,  271,
   242,  242,  242,  242,
    71,  242,  158,  235,
   199,  242,  211,  194,
/* UA.GU..GC */
   242,  242,  242,  242,
   119,  242,  159,  236,
   142,  242,  154,  137,
   133,  242,  145,  250,
/* UA.UA..GC */
   242,  190,  254,  136,
   242,  200,  217,   99,
   242,  141,  233,  115,
   242,  242,  242,  242,
/* UA.UC..GC */
   242,  203,  220,  102,
   242,  179,  243,   78,
   242,  242,  242,  242,
   242,  180,  244,   79,
/* UA.UG..GC */
   242,  162,  254,  136,
   242,  242,  242,  242,
   242,  154,  218,  100,
   242,  207,  177,  181,
/* UA.UU..GC */
   242,  242,  242,  242,
   242,  155,  219,   54,
   242,  150,  120,  124,
   242,   94,  233,   -7,
/* UA.AA..GU */
   233,  185,  192,  242,
   300,  225,  259,  242,
   178,  130,  137,  242,
   242,  242,  242,  242,
/* UA.AC..GU */
   232,  157,  191,  242,
   217,  169,  223,  242,
   242,  242,  242,  242,
   235,  187,  241,  242,
/* UA.AG..GU */
   246,  198,  205,  242,
   242,  242,  242,  242,
   215,  167,  174,  242,
   310,  215,  344,  242,
/* UA.AU..GU */
   242,  242,  242,  242,
   197,  149,  203,  242,
   302,  207,  336,  242,
   202,  107,  236,  242,
/* UA.CA..GU */
   210,  193,  242,  221,
   250,  260,  242,  288,
   155,  185,  242,  213,
   242,  242,  242,  242,
/* UA.CC..GU */
   182,  192,  242,  220,
   194,  177,  242,  205,
   242,  242,  242,  242,
   212,  195,  242,  223,
/* UA.CG..GU */
   223,  253,  242,  281,
   242,  242,  242,  242,
   192,  175,  242,  203,
   240,  270,  242,  298,
/* UA.CU..GU */
   242,  242,  242,  242,
   174,  157,  242,  185,
   232,  262,  242,  290,
   132,  115,  242,  143,
/* UA.GA..GU */
   116,  242,  203,  280,
   183,  242,  270,  300,
    61,  242,  148,  300,
   242,  242,  242,  242,
/* UA.GC..GU */
   115,  242,  202,  232,
   147,  242,  187,  264,
   242,  242,  242,  242,
   165,  242,  205,  282,
/* UA.GG..GU */
   129,  242,  216,  368,
   242,  242,  242,  242,
    98,  242,  185,  262,
   268,  242,  280,  263,
/* UA.GU..GU */
   242,  242,  242,  242,
   127,  242,  167,  244,
   260,  242,  272,  255,
   160,  242,  172,  277,
/* UA.UA..GU */
   242,  199,  263,  145,
   242,  266,  283,  165,
   242,  191,  283,  165,
   242,  242,  242,  242,
/* UA.UC..GU */
   242,  198,  215,   97,
   242,  183,  247,   82,
   242,  242,  242,  242,
   242,  201,  265,  100,
/* UA.UG..GU */
   242,  259,  351,  233,
   242,  242,  242,  242,
   242,  181,  245,  127,
   242,  276,  246,  250,
/* UA.UU..GU */
   242,  242,  242,  242,
   242,  163,  227,   62,
   242,  268,  238,  242,
   242,  121,  260,   20,
/* UA.AA..UG */
   264,  237,  244,  242,
   262,  187,  221,  242,
   168,  120,  127,  242,
   242,  242,  242,  242,
/* UA.AC..UG */
   237,  114,  196,  242,
   245,  197,  251,  242,
   242,  242,  242,  242,
   251,  203,  257,  242,
/* UA.AG..UG */
   244,  196,  182,  242,
   242,  242,  242,  242,
   255,  207,  214,  242,
   315,  220,  349,  242,
/* UA.AU..UG */
   242,  242,  242,  242,
   273,  225,  279,  242,
   332,  237,  366,  242,
   197,  102,  231,  242,
/* UA.CA..UG */
   262,  245,  242,  273,
   164,  222,  242,  250,
   145,  175,  242,  203,
   242,  242,  242,  242,
/* UA.CC..UG */
   187,  197,  242,  225,
   222,  185,  242,  233,
   242,  242,  242,  242,
   228,  211,  242,  239,
/* UA.CG..UG */
   221,  251,  242,  279,
   242,  242,  242,  242,
   232,  215,  242,  243,
   245,  275,  242,  303,
/* UA.CU..UG */
   242,  242,  242,  242,
   250,  233,  242,  240,
   262,  292,  242,  320,
   127,  110,  242,  138,
/* UA.GA..UG */
   168,  242,  255,  332,
   145,  242,  232,  262,
    30,  242,  138,  290,
   242,  242,  242,  242,
/* UA.GC..UG */
   120,  242,  207,  237,
   175,  242,  215,  292,
   242,  242,  242,  242,
   181,  242,  221,  298,
/* UA.GG..UG */
   127,  242,  214,  366,
   242,  242,  242,  242,
   138,  242,  204,  302,
   273,  242,  285,  268,
/* UA.GU..UG */
   242,  242,  242,  242,
   203,  242,  243,  320,
   290,  242,  302,  265,
   155,  242,  167,  272,
/* UA.UA..UG */
   242,  251,  315,  197,
   242,  228,  245,  127,
   242,  181,  273,  155,
   242,  242,  242,  242,
/* UA.UC..UG */
   242,  203,  220,  102,
   242,  211,  275,  110,
   242,  242,  242,  242,
   242,  196,  281,  116,
/* UA.UG..UG */
   242,  257,  349,  231,
   242,  242,  242,  242,
   242,  221,  285,  167,
   242,  281,  231,  255,
/* UA.UU..UG */
   242,  242,  242,  242,
   242,  239,  303,  138,
   242,  298,  268,  272,
   242,  116,  255,   -7,
/* UA.AA..AU */
   233,  185,  192,  242,
   300,  225,  259,  242,
   178,  130,  137,  242,
   242,  242,  242,  242,
/* UA.AC..AU */
   232,  157,  191,  242,
   217,  169,  223,  242,
   242,  242,  242,  242,
   235,  187,  241,  242,
/* UA.AG..AU */
   246,  198,  205,  242,
   242,  242,  242,  242,
   215,  167,  174,  242,
   310,  215,  344,  242,
/* UA.AU..AU */
   242,  242,  242,  242,
   197,  149,  203,  242,
   302,  207,  336,  242,
   202,  107,  236,  242,
/* UA.CA..AU */
   210,  193,  242,  221,
   250,  260,  242,  288,
   155,  185,  242,  213,
   242,  242,  242,  242,
/* UA.CC..AU */
   182,  192,  242,  220,
   194,  177,  242,  205,
   242,  242,  242,  242,
   212,  195,  242,  223,
/* UA.CG..AU */
   223,  253,  242,  281,
   242,  242,  242,  242,
   192,  175,  242,  203,
   240,  270,  242,  298,
/* UA.CU..AU */
   242,  242,  242,  242,
   174,  157,  242,  185,
   232,  262,  242,  290,
   132,  115,  242,  143,
/* UA.GA..AU */
   116,  242,  203,  280,
   183,  242,  270,  300,
    61,  242,  148,  300,
   242,  242,  242,  242,
/* UA.GC..AU */
   115,  242,  202,  232,
   147,  242,  187,  264,
   242,  242,  242,  242,
   165,  242,  205,  282,
/* UA.GG..AU */
   129,  242,  216,  368,
   242,  242,  242,  242,
    98,  242,  185,  262,
   268,  242,  280,  263,
/* UA.GU..AU */
   242,  242,  242,  242,
   127,  242,  167,  244,
   260,  242,  272,  255,
   160,  242,  172,  277,
/* UA.UA..AU */
   242,  199,  263,  145,
   242,  266,  283,  165,
   242,  191,  283,  165,
   242,  242,  242,  242,
/* UA.UC..AU */
   242,  198,  215,   97,
   242,  183,  247,   82,
   242,  242,  242,  242,
   242,  201,  265,  100,
/* UA.UG..AU */
   242,  259,  351,  233,
   242,  242,  242,  242,
   242,  181,  245,  127,
   242,  276,  246,  250,
/* UA.UU..AU */
   242,  242,  242,  242,
   242,  163,  227,   62,
   242,  268,  238,  242,
   242,  121,  260,   20,
/* UA.AA..UA */
   264,  237,  244,  242,
   262,  187,  221,  242,
   168,  120,  127,  242,
   242,  242,  242,  242,
/* UA.AC..UA */
   237,  114,  196,  242,
   245,  197,  251,  242,
   242,  242,  242,  242,
   251,  203,  257,  242,
/* UA.AG..UA */
   244,  196,  182,  242,
   242,  242,  242,  242,
   255,  207,  214,  242,
   315,  220,  349,  242,
/* UA.AU..UA */
   242,  242,  242,  242,
   273,  225,  279,  242,
   332,  237,  366,  242,
   197,  102,  231,  242,
/* UA.CA..UA */
   262,  245,  242,  273,
   164,  222,  242,  250,
   145,  175,  242,  203,
   242,  242,  242,  242,
/* UA.CC..UA */
   187,  197,  242,  225,
   222,  185,  242,  233,
   242,  242,  242,  242,
   228,  211,  242,  239,
/* UA.CG..UA */
   221,  251,  242,  279,
   242,  242,  242,  242,
   232,  215,  242,  243,
   245,  275,  242,  303,
/* UA.CU..UA */
   242,  242,  242,  242,
   250,  233,  242,  240,
   262,  292,  242,  320,
   127,  110,  242,  138,
/* UA.GA..UA */
   168,  242,  255,  332,
   145,  242,  232,  262,
    30,  242,  138,  290,
   242,  242,  242,  242,
/* UA.GC..UA */
   120,  242,  207,  237,
   175,  242,  215,  292,
   242,  242,  242,  242,
   181,  242,  221,  298,
/* UA.GG..UA */
   127,  242,  214,  366,
   242,  242,  242,  242,
   138,  242,  204,  302,
   273,  242,  285,  268,
/* UA.GU..UA */
   242,  242,  242,  242,
   203,  242,  243,  320,
   290,  242,  302,  265,
   155,  242,  167,  272,
/* UA.UA..UA */
   242,  251,  315,  197,
   242,  228,  245,  127,
   242,  181,  273,  155,
   242,  242,  242,  242,
/* UA.UC..UA */
   242,  203,  220,  102,
   242,  211,  275,  110,
   242,  242,  242,  242,
   242,  196,  281,  116,
/* UA.UG..UA */
   242,  257,  349,  231,
   242,  242,  242,  242,
   242,  221,  285,  167,
   242,  281,  231,  255,
/* UA.UU..UA */
   242,  242,  242,  242,
   242,  239,  303,  138,
   242,  298,  268,  272,
   242,  116,  255,   -7,
/* UA.AA.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/* UA.AC.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/* UA.AG.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/* UA.AU.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/* UA.CA.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/* UA.CC.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/* UA.CG.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/* UA.CU.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/* UA.GA.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/* UA.GC.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/* UA.GG.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/* UA.GU.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/* UA.UA.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/* UA.UC.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/* UA.UG.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/* UA.UU.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.AA..CG */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.AC..CG */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.AG..CG */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.AU..CG */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.CA..CG */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.CC..CG */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.CG..CG */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.CU..CG */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.GA..CG */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.GC..CG */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.GG..CG */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.GU..CG */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.UA..CG */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.UC..CG */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.UG..CG */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.UU..CG */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.AA..GC */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.AC..GC */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.AG..GC */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.AU..GC */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.CA..GC */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.CC..GC */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.CG..GC */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.CU..GC */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.GA..GC */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.GC..GC */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.GG..GC */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.GU..GC */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.UA..GC */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.UC..GC */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.UG..GC */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.UU..GC */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.AA..GU */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.AC..GU */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.AG..GU */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.AU..GU */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.CA..GU */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.CC..GU */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.CG..GU */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.CU..GU */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.GA..GU */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.GC..GU */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.GG..GU */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.GU..GU */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.UA..GU */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.UC..GU */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.UG..GU */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.UU..GU */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.AA..UG */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.AC..UG */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.AG..UG */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.AU..UG */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.CA..UG */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.CC..UG */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.CG..UG */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.CU..UG */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.GA..UG */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.GC..UG */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.GG..UG */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.GU..UG */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.UA..UG */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.UC..UG */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.UG..UG */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.UU..UG */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.AA..AU */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.AC..AU */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.AG..AU */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.AU..AU */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.CA..AU */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.CC..AU */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.CG..AU */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.CU..AU */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.GA..AU */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.GC..AU */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.GG..AU */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.GU..AU */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.UA..AU */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.UC..AU */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.UG..AU */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.UU..AU */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.AA..UA */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.AC..UA */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.AG..UA */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.AU..UA */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.CA..UA */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.CC..UA */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.CG..UA */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.CU..UA */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.GA..UA */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.GC..UA */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.GG..UA */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.GU..UA */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.UA..UA */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.UC..UA */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.UG..UA */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.UU..UA */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.AA.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.AC.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.AG.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.AU.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.CA.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.CC.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.CG.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.CU.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.GA.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.GC.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.GG.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.GU.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.UA.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.UC.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.UG.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
/*  @.UU.. @ */
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368,
   368,  368,  368,  368
};

/* hairpin */
static int hairpin37a[] = {
   INF,  INF,  INF,  364,  282,  297,  287,  259,  259,  272,
   283,  293,  303,  311,  319,  327,  334,  340,  346,  352,
   358,  363,  368,  373,  377,  382,  386,  390,  394,  398,
   401
};

/* bulge */
static int bulge37a[] = {
   INF,  281,  157,  201,  288,  298,  272,  288,  303,  315,
   327,  337,  346,  355,  363,  370,  377,  384,  390,  396,
   401,  407,  412,  416,  421,  425,  430,  434,  438,  442,
   445
};

/* internal_loop */
static int internal_loop37a[] = {
   INF,  INF,  INF,  INF,   83,  118,   90,  106,  121,  133,
   145,  155,  164,  173,  181,  188,  195,  202,  208,  214,
   219,  225,  230,  234,  239,  243,  248,  252,  256,  260,
   263
};

/* ML_params */
static int MLparams_a[] = {
/* F = cu*n_unpaired + cc + ci*loop_degree (+TermAU) */
/*	    cu	    cc	    ci	 TerminalAU */
            -2,    315,	    15,	    56
};

/* NINIO */
static int ninio_a[] = {
/* Ninio = MIN(max, m*|n1-n2| */
/*       m   max              */
	 50, 300
};

/* Tetraloops */
struct tetra_loops {
  char *s;
  int e;
} tetraloops_a[] = {
  {"GGGGAC", -33},
  {"GGUGAC", -228},
  {"CGAAAG", -160},
  {"GGAGAC", -85},
  {"CGCAAG", -209},
  {"GGAAAC", -146},
  {"CGGAAG", -146},
  {"CUUCGG", -190},
  {"CGUGAG", -232},
  {"CGAAGG", -219},
  {"CUACGG", -157},
  {"GGCAAC", -153},
  {"CGCGAG", -183},
  {"UGAGAG", -219},
  {"CGAGAG", -55},
  {"AGAAAU", -140},
  {"CGUAAG", -124},
  {"CUAACG", -125},
  {"UGAAAG", -114},
  {"GGAAGC", -82},
  {"GGGAAC", -78},
  {"UGAAAA", -74},
  {"AGCAAU", -118},
  {"AGUAAU", -74},
  {"CGGGAG", -71},
  {"AGUGAU", -107},
  {"GGCGAC", -119},
  {"GGGAGC", -29},
  {"GUGAAC", -40},
  {"UGGAAA", 1},
  {NULL, 0}
};

static void copy_stacks(int stack[NBPAIRS+1][NBPAIRS+1], const int array[])
{
  int i, j, p=0;
  for (i=1; i<=NBPAIRS; i++)
    for (j=1; j<=NBPAIRS; j++)
      stack[i][j] = array[p++];
}

static void copy_loop(int loop[31], const int array[])
{
  int i;
  for (i=0; i<31; i++)
    loop[i] = array[i];
}

static void copy_mismatch(int mismatch[NBPAIRS+1][5][5], const int array[])
{
  int i, j, k, p=0;
  for (i=1; i<=NBPAIRS; i++)
    for (j=0; j<5; j++)
      for (k=0; k<5; k++)
        mismatch[i][j][k] = array[p++];
}

static void copy_int11(int int11[NBPAIRS+1][NBPAIRS+1][5][5], const int array[])
{
  int i, j, k, l, p=0;
  for (i=1; i<NBPAIRS+1; i++)
    for (j=1; j<NBPAIRS+1; j++)
      for (k=0; k<5; k++)
        for (l=0; l<5; l++)
          int11[i][j][k][l] = array[p++];
}

static void copy_int21(int int21[NBPAIRS+1][NBPAIRS+1][5][5][5], const int array[])
{
  int i, j, k, l, m, p=0;
  for (i=1; i<NBPAIRS+1; i++)
    for (j=1; j<NBPAIRS+1; j++)
      for (k=0; k<5; k++)
        for (l=0; l<5; l++)
          for (m=0; m<5; m++)
            int21[i][j][k][l][m] = array[p++];
}

static void copy_int22(int int22[NBPAIRS+1][NBPAIRS+1][5][5][5][5], const int array[])
{
  int i, j, k, l, m, n, p=0;
  for (i=1; i<NBPAIRS+1; i++)
    for (j=1; j<NBPAIRS+1; j++)
      for (k=1; k<5; k++)
	for (l=1; l<5; l++)
	  for (m=1; m<5; m++)
            for (n=1; n<5; n++)
              int22[i][j][k][l][m][n] = array[p++];
}

static void copy_dangle(int dangle[NBPAIRS+1][5], const int array[])
{
  int i, j, p=0;
  for (i=0; i< NBPAIRS+1; i++)
    for (j=0; j<5; j++)
      dangle[i][j] = array[p++];
}

static void copy_MLparams(const int values[])
{
  ML_BASE37 = values[0];
  ML_closing37 = values[1];
  ML_intern37 = values[2];
#ifdef HAVE_VIENNA20
  TerminalAU37 = values[3];
#else
  //TerminalAU = values[3];
#endif
}

static void copy_ninio(const int values[])
{
#ifdef HAVE_VIENNA20
  ninio37 = values[0];
#else
  //F_ninio37[2] = values[0];
#endif
  MAX_NINIO = values[1];
}

static void copy_Tetra_loop(const struct tetra_loops tl[])
{
  int i=0;
  while (tl[i].s!=NULL && i<200) {
    strcpy(&Tetraloops[7*i], tl[i].s);
    strcat(Tetraloops, " ");
#ifdef HAVE_VIENNA20
    Tetraloop37[i] = tl[i].e;
#else
    //TETRA_ENERGY37[i] = tl[i].e;
#endif
    i++;
  }
}

void copy_boltzmann_parameters()
{
  copy_stacks(stack37, stack37a);
  copy_mismatch(mismatchH37, mismatchH37a);
  copy_mismatch(mismatchI37, mismatchI37a);
  copy_dangle(dangle5_37, dangle5_37a);
  copy_dangle(dangle3_37, dangle3_37a);
  copy_int11(int11_37, int11_37a);
  copy_int21(int21_37, int21_37a);
  copy_int22(int22_37, int22_37a);
  copy_loop(hairpin37, hairpin37a);
  copy_loop(bulge37, bulge37a);
  copy_loop(internal_loop37, internal_loop37a);
  copy_MLparams(MLparams_a);
  copy_ninio(ninio_a);
  copy_Tetra_loop(tetraloops_a);
}

