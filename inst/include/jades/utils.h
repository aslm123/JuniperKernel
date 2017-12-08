// Copyright (C) 2017  Spencer Aiello
//
// This file is part of JadesKernel.
//
// jadesKernel is free software: you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
// (at your option) any later version.
//
// JadesKernel is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with JadesKernel.  If not, see <http://www.gnu.org/licenses/>.
#ifndef jades_jades_utils_H
#define jades_jades_utils_H
#include <string>
#include <xeus/xjson.hpp>
#include <zmq.hpp>
#include <Rcpp.h>


using namespace xeus;
template<int SXP, class CTYPE>
static xjson from_sexp(SEXP s) {
  Rcpp::Vector<SXP> rvec = Rcpp::as<Rcpp::Vector<SXP>>(s);

  if( rvec.size()==1 ) {
    return rvec[0];
  }

  std::vector<CTYPE> cvec;
  for(int i=0; i<rvec.size(); ++i)
    cvec.push_back(rvec[i]);
  return cvec;
}

// recursive parse a List into a json
static xjson from_list_r(Rcpp::List lst) {
  if( lst.size()==0 ) return {};
  std::vector<std::string> names = lst.names();
  xjson j;
  int i=0;
  for( Rcpp::List::iterator it = lst.begin(); it!=lst.end(); ++it ) {
    switch( TYPEOF(*it) ) {
    case NILSXP:  { j[names.at(i++)] = nullptr;                         break; }
    case INTSXP:  { j[names.at(i++)] = from_sexp<INTSXP,  int>(*it);    break; }
    case REALSXP: { j[names.at(i++)] = from_sexp<REALSXP, double>(*it); break; }
    case LGLSXP:  { j[names.at(i++)] = from_sexp<LGLSXP,  int>(*it);   break; }
    case VECSXP:  { j[names.at(i++)] = from_list_r(*it);                break; }
    case STRSXP: {
      Rcpp::StringVector tmp = Rcpp::as<Rcpp::StringVector>(*it);
      if( tmp.size()==1 ) {
        j[names.at(i++)] = Rcpp::as<std::string>(*it);
      } else {
        std::vector<std::string> chars;
        std::string skip = "__jades_vec_ignore_hack__";
        for( Rcpp::StringVector::iterator ii=tmp.begin(); ii!=tmp.end(); ++ii ) {
          if( !skip.compare(*ii) ) continue;
          chars.emplace_back(*ii);
        }
        j[names.at(i++)] = chars;
      }
      break;
    }
    default:
      std::stringstream s;
      s << "incompatible SEXP encountered: " << TYPEOF(*it);
      Rcpp::stop(s.str());
    }
  }
  return j;
}

static SEXP from_json_r(xjson j);

template<int SXP, class CTYPE>
static SEXP as_sexp(const xjson& j, bool is_val) {
  if( is_val ) {
    Rcpp::Vector<SXP> res(1);
    res[0] = j.get<CTYPE>();
    return Rcpp::wrap(res);
  }
  Rcpp::Vector<SXP> res(j.size());
  for( size_t i=0; i<j.size(); ++i ) {
    res[i] = j[i].get<CTYPE>();
  }
  return Rcpp::wrap(res);
}


// recursive parse json into list
static SEXP j_to_sexp(const xjson& j, bool is_val=false) {
  if( j.is_null() || (!is_val && j.size()==0) ) return R_NilValue;
  xjson::value_t type = is_val?j.type():j[0].type();
  switch( type ) {
  case xjson::value_t::null:            return R_NilValue;
  case xjson::value_t::boolean:         return as_sexp<LGLSXP, int>(j, is_val);
  case xjson::value_t::string:          return as_sexp<STRSXP,std::string>(j, is_val);
  case xjson::value_t::number_unsigned: /*fall through*/
  case xjson::value_t::number_integer:  return as_sexp<INTSXP, int>(j, is_val);
  case xjson::value_t::number_float:    return as_sexp<REALSXP, double>(j, is_val);
  case xjson::value_t::object:          return from_json_r(j);
  default:
    std::stringstream s;
    s << "don't know what to do with type: " << (int)type;
    Rcpp::stop(s.str());
  }
}


static SEXP from_json_r(xjson j) {
  if( j.is_object() ) {
    Rcpp::List res(j.size());
    Rcpp::StringVector names(j.size());
    int i=0;
    for(xjson::iterator it = j.begin(); it!=j.end(); ++it) {
      names(i) = it.key();
      res[i++] = from_json_r(it.value());
    }
    res.attr("names") = names;
    return Rcpp::wrap(res);
  }
  return j_to_sexp(j, !j.is_array());
}

static std::string msg_t_to_string(const zmq::message_t& msg) {
  std::stringstream ss;
  const char* chars = msg.data<char>();
  size_t sz = msg.size();
  for(size_t i=0; i<sz; ++i)
    ss << chars[i];
  return ss.str();
}
#endif // #ifndef jades_jades_utils_H