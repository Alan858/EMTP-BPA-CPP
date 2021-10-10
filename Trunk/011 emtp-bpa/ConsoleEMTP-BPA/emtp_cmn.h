/***********************************
EMTP C++ 
translated from BPA-EMTP https://github.com/Alan858/BPA_EMTP
using Fable https://cci.lbl.gov/fable/
version 0.5.0
https://github.com/Alan858/EMTP-BPA-CPP

Licensed under the MIT License <https://opensource.org/licenses/MIT>
  Copyright(c) 2021, Dr. Alan W. Zhang <alan92127@gmail.com>

  Permission is hereby  granted, free of charge, to any person obtaining a copy
  of this softwareand associated documentation files(the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and /or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
********************************************************/




#include <iostream>
#include <fstream>
#include <cstdlib>
#include <cmath>
#include <string>
#include <iomanip>
#include <filesystem>
#include <vector>
#include <array>
#include <charconv>
#include <random>
#include <cassert>
#include <unordered_map>
#pragma warning (disable: 4267 4297)
#include <fem.hpp> // Fortran EMulation library of fable module



namespace emtp {

  template<typename T, size_t N>
  struct arrayEx : public std::array< T, N >
  {
    std::array< T, N >::reference operator()(int ind)
    {
#ifdef _DEBUG
      return std::array::at(ind - 1);
#else
      return (*this)[ind - 1];
#endif
    }
    std::array< T, N >::const_reference operator()(int ind) const
    {
#ifdef _DEBUG
      return std::array::at(ind - 1);
#else
      return (*this)[ind - 1];
#endif
    }
  };

  template<typename T>
  struct vectorEx : public std::vector<T>
  {
    using std::vector<T>::vector;
    auto& operator()(int ind)
    {
#ifdef _DEBUG
      assert(0 < ind && ind <= this->size());
      return std::vector<T>::at(ind - 1);
#else
      return std::vector<T>::operator[](ind - 1); //(*this)[ind - 1];
#endif
    }
    const auto& operator()(int ind) const
    {
#ifdef _DEBUG
      return std::vector<T>::at(ind - 1);
#else
      return std::vector<T>::operator[](ind - 1); //(*this)[ind - 1];
#endif
    }
  };

  template<class T>
  class ArraySpan {
    T* d_;
    int s_;
  public:
    ArraySpan() = delete;
    ArraySpan(T* data, int size)
      : d_(data)
      , s_(size)
    {}
    T& operator()(int idx) {
      if (!d_ || idx < 1 || s_ < idx) throw std::range_error("in ArraySpan");
      return d_[idx - 1];
    }
    auto size() const {
      return s_;
    }
  };

  class SState {
    const std::string fmt_;
  public:
    SState(std::string_view fmt) : fmt_(fmt)
    {
    }
    friend std::ostream& operator << (std::ostream& o, const SState& ss) {
      std::string_view fmt = ss.fmt_;
      if (fmt.empty()) {  // default
        std::ostringstream t;
        o.flags(t.flags());
        o.width(t.width());
        o.precision(t.precision());
        o.fill(t.fill());
      }
      else {
        switch (fmt[0]) {
        case 'f': o << std::fixed; break;
        case 'e': o << std::scientific; break;
        case 'g': o << std::defaultfloat; break;
        default:
          throw std::runtime_error("unhandled format!");
        }
        fmt.remove_prefix(1);
        auto pos = fmt.find('.');
        if (pos == std::string::npos) {
          if (!fmt.empty())
            o.width(std::stoi(std::string(fmt)));
        }
        else {
          int w = 0, preci = 6;
          if (std::from_chars(fmt.data(), fmt.data() + pos, w).ptr != fmt.data())
            o.width(w);
          fmt.remove_prefix(pos + 1);
          if (std::from_chars(fmt.data(), fmt.data() + fmt.size(), preci).ptr != fmt.data())
            o.precision(preci);
        }
      }
      return o;
    }
  };

  inline std::string getCurrentDateTime() {
    //auto now = std::chrono::system_clock::now();
    //std::time_t nowTime = std::chrono::system_clock::to_time_t(now);
    //return std::ctime(&nowTime);
    std::time_t t = std::time(0);   // get time now
    std::tm buf;
    ::localtime_s(&buf, &t);
    std::stringstream ss;
    ss << (buf.tm_year + 1900) << std::setfill('0')
      << '-' << std::setw(2) << (buf.tm_mon + 1)
      << '-' << std::setw(2) << buf.tm_mday
      << ' ' << std::setw(2) << buf.tm_hour
      << ':' << std::setw(2) << buf.tm_min
      << ':' << std::setw(2) << buf.tm_sec;
    return ss.str();
  }

  inline std::string random_string(const int length = 24) {
    std::string str("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz");
    std::mt19937 generator(std::random_device{}());
    std::shuffle(str.begin(), str.end(), generator);
    return str.substr(0, length);    // assumes 32 < number of characters in str         
  }

  inline std::string_view trim_left(const std::string_view strv) {
    return strv.substr(std::min(strv.find_first_not_of(' '), strv.size()));
  }
  inline std::string_view trim_right(const std::string_view strv) {
    return strv.substr(0, strv.find_last_not_of(' ') + 1);
  }
  inline std::string_view trim(const std::string_view strv) {
    return trim_left(trim_right(strv));
  }

  template<typename T>
  T read(std::string_view v) {
    T d{};
    v = trim(v);
    std::from_chars(v.data(), v.data() + v.size(), d);
    return d;
  }

  inline std::vector<std::string_view> split(const std::string_view strv, const std::string_view delim) {
    std::vector<std::string_view> result;
    size_t pos = 0;
    size_t len = 0;
    while ((len = strv.substr(pos).find(delim)) != std::string::npos) {
      if (0 < len)
        result.push_back(strv.substr(pos, len));
      pos += len + delim.length();
    }
    if (len = strv.size() - pos; 0 < len)
      result.push_back(strv.substr(pos, len));
    return result;
  }



  using namespace fem::major_types;

  template<typename T>
  T absz(T x) {
    return std::abs(x);
  }

  template<typename T>
  T acosz(T x) {
    return std::acos(x);
  }

  inline double
    aimagz(const std::complex<double>& x)
  {
    return x.imag();
  }

  inline void
    cdsqrt(...)
  {
    throw std::runtime_error(
      "Missing function implementation: cdsqrt");
  }

  template<typename T>
  T cosz(T x) {
    return std::cos(x);
  }

  inline double
    alog1z(double x) {
    return log10(x);
  }

  inline double
    alogz(double x) {
    return std::log(x);
  }


  inline double
    cabsz(std::complex<double> x)
  {
    return std::abs(x);
  }

  inline std::complex<double>
    cdexp(std::complex<double> x)
  {
    return std::exp(x);
  }

  inline std::complex<double>
    cdlog(std::complex<double> x)
  {
    return std::log(x);
  }

  inline std::complex<double>
    cdsqrt(std::complex<double> x)
  {
    return std::sqrt(x);
  }

  //inline double
  //  cosz(double x)
  //{
  //  return std::cos(x);
  //}

  inline double
    cotanz(double x) {
    return 1. / std::tan(x);
  }

  inline void
    datan(...)
  {
    throw std::runtime_error(
      "Missing function implementation: datan");
  }

  inline void
    datn2z(...)
  {
    throw std::runtime_error(
      "Missing function implementation: datn2z");
  }

  inline void
    dcosh(...)
  {
    throw std::runtime_error(
      "Missing function implementation: dcosh");
  }

  inline void
    dcosz(...)
  {
    throw std::runtime_error(
      "Missing function implementation: dcosz");
  }

  //inline void
  //  dexpz(...)
  //{
  //  throw std::runtime_error(
  //    "Missing function implementation: dexpz");
  //}

  //inline void
  //  dint(...)
  //{
  //  throw std::runtime_error(
  //    "Missing function implementation: dint");
  //}

  inline double
    dmod(double d1, double d2)
  {
    return std::fmod(d1, d2);
  }

  inline void
    dsinh(...)
  {
    throw std::runtime_error(
      "Missing function implementation: dsinh");
  }

  inline void
    dsinz(...)
  {
    throw std::runtime_error(
      "Missing function implementation: dsinz");
  }

  inline void
    dsqrtz(double d1, double& d2)
  {
    d2 = std::sqrt(d1);
  }

  inline double
    dtanh(double x)
  {
    return std::tanh(x);
  }

  inline double
    expz(double x) {
    return std::exp(x);
  }

  inline void
    idint(...)
  {
    throw std::runtime_error(
      "Missing function implementation: idint");
  }

  inline void
    imag(...)
  {
    throw std::runtime_error(
      "Missing function implementation: imag");
  }

  //int loc(std::string& v) // return pointer
  //{
  //  return 1; // static_cast<int>(*v);
  //}

  inline double
    realz(std::complex<double> x)
  {
    return x.real();
  }

  inline void
    setplt(...)
  {
    throw std::runtime_error(
      "Missing function implementation: setplt");
  }

  inline void
    setstd(...)
  {
    throw std::runtime_error(
      "Missing function implementation: setstd");
  }

  inline double
    sinhz(double x) {
    return std::sinh(x);
  }

  inline double
    sinz(double x) {
    return std::sin(x);
  }

  inline void
    spytac(...)
  {
    throw std::runtime_error(
      "Missing function implementation: spytac");
  }

  template<typename T>
  T sqrtz(T x) {
    return std::sqrt(x);
  }

  //inline double
  //  tanhz(double x)
  //{
  //  return std::tanh(x);
  //}

  //inline double
  //  tanz(double x) {
  //  return std::tan(x);
  //}

  inline float
    ustart(...)
  {
    throw std::runtime_error(
      "Missing function implementation: ustart");
  }

/* Dependency cycles: 1
     emtspy spying spyink cimage frefld freone tacs1 tacs1a
 */

struct common_cmn
{
  fem::str<8> bus1;
  fem::str<8> bus2;
  fem::str<8> bus3;
  fem::str<8> bus4;
  fem::str<8> bus5;
  fem::str<8> bus6;
  fem::str<8> trash;
  fem::str<8> blank;
  fem::str<8> terra;
  fem::str<8> userid;
  fem::str<8> branch;
  fem::str<8> copy;
  fem::str<8> csepar;
  fem::str<8> chcont;
  vectorEx<fem::str<8> > texcol;
  vectorEx<fem::str<8> > texta6;
  vectorEx<fem::str<8> > date1;
  vectorEx<fem::str<8> > tclock;
  vectorEx<fem::str<8> > vstacs;
  fem::str<80> abuff;
  double ci1;
  double ck1;
  double deltat;
  double delta2;
  double freqcs;
  double epsiln;
  double xunits;
  double aincr;
  double xmaxmx;
  arr<double> znolim;
  double epszno;
  double epwarn;
  double epstop;
  double t;
  double tolmat;
  const double pi;
  const double twopi;
  double tmax;
  double omega;
  double copt;
  double xopt;
  double szplt;
  double szbed;
  double sglfir;
  double sigmax;
  double epsuba;
  double epdgel;
  double epomeg;
  double fminfs;
  double delffs;
  double fmaxfs;
  double tenerg;
  arr<double> begmax;
  double tenm3;
  double tenm6;
  double unity;
  double onehaf;
  arr<double> peaknd;
  double fltinf;
  double flzero;
  double degmin;
  double degmax;
  double statfr;
  arr<double> voltbc;
  arr<double> flstat;
  double angle;
  double pu;
  double dltinv;
  double speedl;
  arr<int> moncar;
  const int lunit1;
  const int lunit2;
  const int lunit3;
  const int lunit4;
  const int lunit5;
  const int lunit6;
  const int lunit7;
  const int lunit8;
  const int lunit9;
  const int lunt10;
  const int lunt11;
  const int lunt12;
  const int lunt13;
  const int lunt14;
  const int lunt15;
  int nright;
  int nfrfld;
  int kolbeg;
  int max99m;
  arr<int> kprchg;
  arr<int> multpr;
  arr<int> ipntv;
  arr<int> indtv;
  arr<int> lstat;
  arr<int> nbyte;
  arr<int> lunsav;
  arr<int> iprsov;
  int icheck;
  int iline;
  int inonl;
  int iout;
  int ipunch;
  int iread;
  int kol132;
  int istep;
  int kwtspy;
  int itype;
  int it1;
  int it2;
  int izero;
  int kcount;
  int istead;
  int ldata;
  int lbrnch;
  int lexct;
  int lbus;
  int lymat;
  int lswtch;
  int lnonl;
  int lchar;
  int m4plot;
  int lpast;
  int lsize7;
  int iplot;
  int ncomp;
  int nv;
  int lcomp;
  int numsm;
  int ifdep;
  int ltails;
  int lfdep;
  int lwt;
  int last;
  int npower;
  int maxpe;
  int lsiz12;
  int lsmout;
  int limass;
  int iv;
  arr<int> ktrlsw;
  int num99;
  int kpartb;
  int llbuff;
  int kanal;
  int nsmth;
  int ntcsex;
  int nstacs;
  int maxbus;
  int lastov;
  int ltacst;
  int lhist;
  int ifx;
  int isubc1;
  int inecho;
  int noutpr;
  int ktab;
  int jflsos;
  int numdcd;
  int numum;
  int lspcum;
  int nphcas;
  int ialter;
  int ichar;
  int ktref;
  int memsav;
  int lisoff;
  int kburro;
  int iaverg;
  int lsiz23;
  int lsiz26;
  int numout;
  int moldat;
  int lsiz27;
  int lsiz28;
  int ltlabl;
  int iwt;
  int ifdep2;
  int idoubl;
  int ioutin;
  int ipun;
  int jst;
  int jst1;
  arr<int> muntsv;
  int numsub;
  int maxzno;
  int ifsem;
  int lfsem;
  int iadd;
  int lfd;
  arr<int> nexout;
  int iofgnd;
  int iofbnd;
  int modout;
  int lint;
  int iftail;
  int ncurr;
  int ioffd;
  int isplot;
  int isprin;
  int maxout;
  int kill;
  int ivolt;
  int nchain;
  int iprsup;
  int intinf;
  int kconst;
  int kswtch;
  int it;
  int ntot;
  int ibr;
  int lsyn;
  int kssout;
  arr<int> loopss;
  int numref;
  int nword1;
  int nword2;
  int iloaep;
  int lnpin;
  int ntot1;
  int limstp;
  int indstp;
  int nc;
  int icat;
  int numnvo;
  int nenerg;

  common_cmn() :
    bus1(""),
    bus2(""),
    bus3(""),
    bus4(""),
    bus5(""),
    bus6(""),
    trash(""),
    blank(""),
    terra(""),
    userid(""),
    branch(""),
    copy(""),
    csepar(""),
    chcont(""),
    //texcol(dimension(80), fem::fill0),
    //texta6(dimension(15), fem::fill0),
    //date1(dimension(2), fem::fill0),
    //tclock(dimension(2), fem::fill0),
    //vstacs(dimension(24), fem::fill0),
    texcol(80, ""),
    texta6(15, ""),
    date1(2, ""),
    tclock(2, ""),
    vstacs(24, ""),
    //abuff(dimension(20), fem::fill0),
    ci1(fem::double0),
    ck1(fem::double0),
    deltat(fem::double0),
    delta2(fem::double0),
    freqcs(fem::double0),
    epsiln(fem::double0),
    xunits(fem::double0),
    aincr(fem::double0),
    xmaxmx(fem::double0),
    znolim(dimension(2), fem::fill0),
    epszno(fem::double0),
    epwarn(fem::double0),
    epstop(fem::double0),
    t(fem::double0),
    tolmat(fem::double0),
    pi(3.14159265358979323846),
    twopi(2 * 3.14159265358979323846),
    tmax(fem::double0),
    omega(fem::double0),
    copt(fem::double0),
    xopt(fem::double0),
    szplt(fem::double0),
    szbed(fem::double0),
    sglfir(fem::double0),
    sigmax(fem::double0),
    epsuba(fem::double0),
    epdgel(fem::double0),
    epomeg(fem::double0),
    fminfs(fem::double0),
    delffs(fem::double0),
    fmaxfs(fem::double0),
    tenerg(fem::double0),
    begmax(dimension(6), fem::fill0),
    tenm3(fem::double0),
    tenm6(fem::double0),
    unity(fem::double0),
    onehaf(fem::double0),
    peaknd(dimension(3), fem::fill0),
    fltinf(fem::double0),
    flzero(fem::double0),
    degmin(fem::double0),
    degmax(fem::double0),
    statfr(fem::double0),
    voltbc(dimension(50), fem::fill0),
    flstat(dimension(20), fem::fill0),
    angle(fem::double0),
    pu(fem::double0),
    dltinv(fem::double0),
    speedl(fem::double0),
    moncar(dimension(10), fem::fill0),
    lunit1(1),
    lunit2(2),
    lunit3(3),
    lunit4(4),
    lunit5(55), // 0, 5, 6 are console screen
    lunit6(56),
    lunit7(7),
    lunit8(8),
    lunit9(9),
    lunt10(10),
    lunt11(11),
    lunt12(12),
    lunt13(13),
    lunt14(14),
    lunt15(15),
    nright(fem::int0),
    nfrfld(fem::int0),
    kolbeg(fem::int0),
    max99m(fem::int0),
    kprchg(dimension(6), fem::fill0),
    multpr(dimension(5), fem::fill0),
    ipntv(dimension(11), fem::fill0),
    indtv(dimension(10), fem::fill0),
    lstat(dimension(80), fem::fill0),
    nbyte(dimension(6), fem::fill0),
    lunsav(dimension(15), fem::fill0),
    iprsov(dimension(39), fem::fill0),
    icheck(fem::int0),
    iline(fem::int0),
    inonl(fem::int0),
    iout(fem::int0),
    ipunch(fem::int0),
    iread(fem::int0),
    kol132(fem::int0),
    istep(fem::int0),
    kwtspy(fem::int0),
    itype(fem::int0),
    it1(fem::int0),
    it2(fem::int0),
    izero(fem::int0),
    kcount(fem::int0),
    istead(fem::int0),
    ldata(fem::int0),
    lbrnch(fem::int0),
    lexct(fem::int0),
    lbus(fem::int0),
    lymat(fem::int0),
    lswtch(fem::int0),
    lnonl(fem::int0),
    lchar(fem::int0),
    m4plot(fem::int0),
    lpast(fem::int0),
    lsize7(fem::int0),
    iplot(fem::int0),
    ncomp(fem::int0),
    nv(fem::int0),
    lcomp(fem::int0),
    numsm(fem::int0),
    ifdep(fem::int0),
    ltails(fem::int0),
    lfdep(fem::int0),
    lwt(fem::int0),
    last(fem::int0),
    npower(fem::int0),
    maxpe(fem::int0),
    lsiz12(fem::int0),
    lsmout(fem::int0),
    limass(fem::int0),
    iv(fem::int0),
    ktrlsw(dimension(8), fem::fill0),
    num99(fem::int0),
    kpartb(fem::int0),
    llbuff(fem::int0),
    kanal(fem::int0),
    nsmth(fem::int0),
    ntcsex(fem::int0),
    nstacs(fem::int0),
    maxbus(fem::int0),
    lastov(fem::int0),
    ltacst(fem::int0),
    lhist(fem::int0),
    ifx(fem::int0),
    isubc1(fem::int0),
    inecho(fem::int0),
    noutpr(fem::int0),
    ktab(fem::int0),
    jflsos(fem::int0),
    numdcd(fem::int0),
    numum(fem::int0),
    lspcum(fem::int0),
    nphcas(fem::int0),
    ialter(fem::int0),
    ichar(fem::int0),
    ktref(fem::int0),
    memsav(fem::int0),
    lisoff(fem::int0),
    kburro(fem::int0),
    iaverg(fem::int0),
    lsiz23(fem::int0),
    lsiz26(fem::int0),
    numout(fem::int0),
    moldat(fem::int0),
    lsiz27(fem::int0),
    lsiz28(fem::int0),
    ltlabl(fem::int0),
    iwt(fem::int0),
    ifdep2(fem::int0),
    idoubl(fem::int0),
    ioutin(fem::int0),
    ipun(fem::int0),
    jst(fem::int0),
    jst1(fem::int0),
    muntsv(dimension(2), fem::fill0),
    numsub(fem::int0),
    maxzno(fem::int0),
    ifsem(fem::int0),
    lfsem(fem::int0),
    iadd(fem::int0),
    lfd(fem::int0),
    nexout(dimension(17), fem::fill0),
    iofgnd(fem::int0),
    iofbnd(fem::int0),
    modout(fem::int0),
    lint(fem::int0),
    iftail(fem::int0),
    ncurr(fem::int0),
    ioffd(fem::int0),
    isplot(fem::int0),
    isprin(fem::int0),
    maxout(fem::int0),
    kill(fem::int0),
    ivolt(fem::int0),
    nchain(fem::int0),
    iprsup(fem::int0),
    intinf(fem::int0),
    kconst(fem::int0),
    kswtch(fem::int0),
    it(fem::int0),
    ntot(fem::int0),
    ibr(fem::int0),
    lsyn(fem::int0),
    kssout(fem::int0),
    loopss(dimension(13), fem::fill0),
    numref(fem::int0),
    nword1(fem::int0),
    nword2(fem::int0),
    iloaep(fem::int0),
    lnpin(fem::int0),
    ntot1(fem::int0),
    limstp(fem::int0),
    indstp(fem::int0),
    nc(fem::int0),
    icat(fem::int0),
    numnvo(fem::int0),
    nenerg(fem::int0)
  {}
};

struct common_comthl
{
  double angtpe;
  int nswtpe;

  common_comthl() :
    angtpe(fem::double0),
    nswtpe(fem::int0)
  {}
};

struct common_comld
{
  int newtac;

  common_comld() :
    newtac(fem::int0)
  {}
};

struct common_c29b01
{
  arr<int> karray;
  ArraySpan<double> farray;
  common_c29b01() :
    karray(dimension(1992869), fem::fill0)
    , farray(reinterpret_cast<double*>(karray.begin()), karray.size() / 2)
  {}
};

struct common_c0b001
{
  arr<double> x;

  common_c0b001() :
    x(dimension(10000), fem::fill0)
  {}
};

struct common_c0b002
{
  arr<double> ykm;

  common_c0b002() :
    ykm(dimension(20000), fem::fill0)
  {}
};

struct common_c0b003
{
  arr<int> km;

  common_c0b003() :
    km(dimension(20000), fem::fill0)
  {}
};

struct common_c0b004
{
  arr<double> xk;

  common_c0b004() :
    xk(dimension(121080), fem::fill0)
  {}
};

struct common_c0b005
{
  arr<double> xm;

  common_c0b005() :
    xm(dimension(121080), fem::fill0)
  {}
};

struct common_c0b006
{
  arr<double> weight;

  common_c0b006() :
    weight(dimension(460), fem::fill0)
  {}
};

struct common_c0b007
{
  arr<int> iwtent;

  common_c0b007() :
    iwtent(dimension(31), fem::fill0)
  {}
};

struct common_c0b008
{
  arr<double> con1;

  common_c0b008() :
    con1(dimension(30), fem::fill0)
  {}
};

struct common_c0b009
{
  arr<int> iskip;

  common_c0b009() :
    iskip(dimension(5), fem::fill0)
  {}
};

struct common_c0b010
{
  arr<double> zinf;

  common_c0b010() :
    zinf(dimension(5), fem::fill0)
  {}
};

struct common_c0b011
{
  arr<double> eta;

  common_c0b011() :
    eta(dimension(5), fem::fill0)
  {}
};

struct common_c0b012
{
  arr<int> nhist;

  common_c0b012() :
    nhist(dimension(5), fem::fill0)
  {}
};

struct common_c0b013
{
  arr<double> stailm;

  common_c0b013() :
    stailm(dimension(90), fem::fill0)
  {}
};

struct common_c0b014
{
  arr<double> stailk;

  common_c0b014() :
    stailk(dimension(90), fem::fill0)
  {}
};

struct common_c0b015
{
  arr<double> xmax;

  common_c0b015() :
    xmax(dimension(3600), fem::fill0)
  {}
};

struct common_c0b016
{
  arr<int> koutvp;

  common_c0b016() :
    koutvp(dimension(508), fem::fill0)
  {}
};

struct common_c0b017
{
  arr<double> bnrg;

  common_c0b017() :
    bnrg(dimension(254), fem::fill0)
  {}
};

struct common_c0b018
{
  arr<double> sconst;

  common_c0b018() :
    sconst(dimension(100000), fem::fill0)
  {}
};

struct common_c0b019
{
  arr<double> cnvhst;

  common_c0b019() :
    cnvhst(dimension(53000), fem::fill0)
  {}
};

struct common_c0b020
{
  arr<double> sfd;

  common_c0b020() :
    sfd(dimension(6000), fem::fill0)
  {}
};

struct common_c0b021
{
  arr<double> qfd;

  common_c0b021() :
    qfd(dimension(6000), fem::fill0)
  {}
};

struct common_c0b022
{
  arr<double> semaux;

  common_c0b022() :
    semaux(dimension(50000), fem::fill0)
  {}
};

struct common_c0b023
{
  arr<int> ibsout;

  common_c0b023() :
    ibsout(dimension(900), fem::fill0)
  {}
};

struct common_c0b024
{
  arr<double> bvalue;

  common_c0b024() :
    bvalue(dimension(900), fem::fill0)
  {}
};

struct common_c0b025
{
  arr<double> sptacs;
  ArraySpan<int> isptacs;
  common_c0b025() :
    sptacs(dimension(90000), fem::fill0)
    , isptacs(reinterpret_cast<int*>(sptacs.begin()), sptacs.size() * 2)
  {}
};

struct common_c0b026
{
  arr<int> kswtyp;

  common_c0b026() :
    kswtyp(dimension(1200), fem::fill0)
  {}
};

struct common_c0b027
{
  arr<int> modswt;

  common_c0b027() :
    modswt(dimension(1200), fem::fill0)
  {}
};

struct common_c0b028
{
  arr<int> kbegsw;

  common_c0b028() :
    kbegsw(dimension(1200), fem::fill0)
  {}
};

struct common_c0b029
{
  arr<int> lastsw;

  common_c0b029() :
    lastsw(dimension(1200), fem::fill0)
  {}
};

struct common_c0b030
{
  arr<int> kentnb;

  common_c0b030() :
    kentnb(dimension(1200), fem::fill0)
  {}
};

struct common_c0b031
{
  arr<int> nbhdsw;

  common_c0b031() :
    nbhdsw(dimension(3600), fem::fill0)
  {}
};

struct common_c0b032
{
  arr<double> topen;

  common_c0b032() :
    topen(dimension(3600), fem::fill0)
  {}
};

struct common_c0b033
{
  arr<double> crit;

  common_c0b033() :
    crit(dimension(1200), fem::fill0)
  {}
};

struct common_c0b034
{
  arr<int> kdepsw;

  common_c0b034() :
    kdepsw(dimension(3600), fem::fill0)
  {}
};

struct common_c0b035
{
  arr<double> tdns;

  common_c0b035() :
    tdns(dimension(1200), fem::fill0)
  {}
};

struct common_c0b036
{
  arr<int> isourc;

  common_c0b036() :
    isourc(dimension(1200), fem::fill0)
  {}
};

struct common_c0b037
{
  arr<double> energy;

  common_c0b037() :
    energy(dimension(1200), fem::fill0)
  {}
};

struct common_c0b038
{
  arr<int> iardub;

  common_c0b038() :
    iardub(dimension(3600), fem::fill0)
  {}
};

struct common_c0b039
{
  arr<double> ardube;

  common_c0b039() :
    ardube(dimension(4800), fem::fill0)
  {}
};

struct common_c0b040
{
  arr<int> nonlad;

  common_c0b040() :
    nonlad(dimension(300), fem::fill0)
  {}
};

struct common_c0b041
{
  arr<int> nonle;

  common_c0b041() :
    nonle(dimension(300), fem::fill0)
  {}
};

struct common_c0b042
{
  arr<double> vnonl;

  common_c0b042() :
    vnonl(dimension(300), fem::fill0)
  {}
};

struct common_c0b043
{
  arr<double> curr;

  common_c0b043() :
    curr(dimension(300), fem::fill0)
  {}
};

struct common_c0b044
{
  arr<double> anonl;

  common_c0b044() :
    anonl(dimension(300), fem::fill0)
  {}
};

struct common_c0b045
{
  arr<double> vecnl1;

  common_c0b045() :
    vecnl1(dimension(300), fem::fill0)
  {}
};

struct common_c0b046
{
  arr<double> vecnl2;

  common_c0b046() :
    vecnl2(dimension(300), fem::fill0)
  {}
};

struct common_c0b047
{
  arr<int> namenl;

  common_c0b047() :
    namenl(dimension(300), fem::fill0)
  {}
};

struct common_c0b048
{
  arr<double> vzer5;

  common_c0b048() :
    vzer5(dimension(300), fem::fill0)
  {}
};

struct common_c0b049
{
  arr<int> ilast;

  common_c0b049() :
    ilast(dimension(300), fem::fill0)
  {}
};

struct common_c0b050
{
  arr<int> nltype;

  common_c0b050() :
    nltype(dimension(300), fem::fill0)
  {}
};

struct common_c0b051
{
  arr<int> kupl;

  common_c0b051() :
    kupl(dimension(300), fem::fill0)
  {}
};

struct common_c0b052
{
  arr<int> nlsub;

  common_c0b052() :
    nlsub(dimension(300), fem::fill0)
  {}
};

struct common_c0b053
{
  arr<double> xoptbr;

  common_c0b053() :
    xoptbr(dimension(3000), fem::fill0)
  {}
};

struct common_c0b054
{
  arr<double> coptbr;

  common_c0b054() :
    coptbr(dimension(3000), fem::fill0)
  {}
};

struct common_c0b055
{
  arr<double> cursub;

  common_c0b055() :
    cursub(dimension(312), fem::fill0)
  {}
};

struct common_c0b056
{
  arr<double> cchar;

  common_c0b056() :
    cchar(dimension(900), fem::fill0)
  {}
};

struct common_c0b057
{
  arr<double> vchar;

  common_c0b057() :
    vchar(dimension(900), fem::fill0)
  {}
};

struct common_c0b058
{
  arr<double> gslope;

  common_c0b058() :
    gslope(dimension(900), fem::fill0)
  {}
};

struct common_c0b059
{
  arr<int> ktrans;

  common_c0b059() :
    ktrans(dimension(3002), fem::fill0)
  {}
};

struct common_c0b060
{
  arr<int> kk;

  common_c0b060() :
    kk(dimension(3002), fem::fill0)
  {}
};

struct common_c0b061
{
  arr<double> c;

  common_c0b061() :
    c(dimension(10000), fem::fill0)
  {}
};

struct common_c0b062
{
  arr<double> tr;

  common_c0b062() :
    tr(dimension(20000), fem::fill0)
  {}
};

struct common_c0b063
{
  arr<double> tx;

  common_c0b063() :
    tx(dimension(20000), fem::fill0)
  {}
};

struct common_c0b064
{
  arr<double> r;

  common_c0b064() :
    r(dimension(10000), fem::fill0)
  {}
};

struct common_c0b065
{
  arr<int> nr;

  common_c0b065() :
    nr(dimension(3000), fem::fill0)
  {}
};

struct common_c0b066
{
  arr<int> length;

  common_c0b066() :
    length(dimension(3000), fem::fill0)
  {}
};

struct common_c0b067
{
  arr<double> cik;

  common_c0b067() :
    cik(dimension(3000), fem::fill0)
  {}
};

struct common_c0b068
{
  arr<double> ci;

  common_c0b068() :
    ci(dimension(3000), fem::fill0)
  {}
};

struct common_c0b069
{
  arr<double> ck;

  common_c0b069() :
    ck(dimension(3000), fem::fill0)
  {}
};

struct common_c0b070
{
  arr<int> ismout;

  common_c0b070() :
    ismout(dimension(1052), fem::fill0)
  {}
};

struct common_c0b071
{
  arr<double> elp;

  common_c0b071() :
    elp(dimension(404), fem::fill0)
  {}
};

struct common_c0b072
{
  arr<double> cu;

  common_c0b072() :
    cu(dimension(96), fem::fill0)
  {}
};

struct common_c0b073
{
  arr<double> shp;

  common_c0b073() :
    shp(dimension(1008), fem::fill0)
  {}
};

struct common_c0b074
{
  arr<double> histq;

  common_c0b074() :
    histq(dimension(504), fem::fill0)
  {}
};

struct common_c0b075
{
  arr<int> ismdat;

  common_c0b075() :
    ismdat(dimension(120), fem::fill0)
  {}
};

struct common_c0b076
{
  arr<fem::str<8> > texvec;

  common_c0b076() :
    texvec(dimension(4000), fem::fill0)
  {}
};

struct common_c0b077
{
  arr<int> ibrnch;

  common_c0b077() :
    ibrnch(dimension(900), fem::fill0)
  {}
};

struct common_c0b078
{
  arr<int> jbrnch;

  common_c0b078() :
    jbrnch(dimension(900), fem::fill0)
  {}
};

struct common_c0b079
{
  arr<double> tstop;

  common_c0b079() :
    tstop(dimension(100), fem::fill0)
  {}
};

struct common_c0b080
{
  arr<int> nonlk;

  common_c0b080() :
    nonlk(dimension(300), fem::fill0)
  {}
};

struct common_c0b081
{
  arr<int> nonlm;

  common_c0b081() :
    nonlm(dimension(300), fem::fill0)
  {}
};

struct common_c0b082
{
  arr<double> spum;
  ArraySpan<int> ispum;
  common_c0b082() :
    spum(dimension(30000), fem::fill0)
    , ispum(reinterpret_cast<int*>(spum.begin()), spum.size() * 2)
  {}
};

struct common_c0b083
{
  arr<int> kks;

  common_c0b083() :
    kks(dimension(3002), fem::fill0)
  {}
};

struct common_c0b084
{
  arr<int> kknonl;

  common_c0b084() :
    kknonl(dimension(72048), fem::fill0)
  {}
};

struct common_c0b085
{
  arr<double> znonl;

  common_c0b085() :
    znonl(dimension(72048), fem::fill0)
  {}
};

struct common_c0b086
{
  arr<double> znonlb;

  common_c0b086() :
    znonlb(dimension(3002), fem::fill0)
  {}
};

struct common_c0b087
{
  arr<double> znonlc;

  common_c0b087() :
    znonlc(dimension(3002), fem::fill0)
  {}
};

struct common_c0b088
{
  arr<double> finit;

  common_c0b088() :
    finit(dimension(3002), fem::fill0)
  {}
};

struct common_c0b089
{
  arr<int> ksub;

  common_c0b089() :
    ksub(dimension(312), fem::fill0)
  {}
};

struct common_c0b090
{
  arr<int> msub;

  common_c0b090() :
    msub(dimension(312), fem::fill0)
  {}
};

struct common_c0b091
{
  arr<int> isubeg;

  common_c0b091() :
    isubeg(dimension(304), fem::fill0)
  {}
};

struct common_c0b092
{
  arr<int> litype;

  common_c0b092() :
    litype(dimension(3000), fem::fill0)
  {}
};

struct common_c0b093
{
  arr<int> imodel;

  common_c0b093() :
    imodel(dimension(3000), fem::fill0)
  {}
};

struct common_c0b094
{
  arr<int> kbus;

  common_c0b094() :
    kbus(dimension(3000), fem::fill0)
  {}
};

struct common_c0b095
{
  arr<int> mbus;

  common_c0b095() :
    mbus(dimension(3000), fem::fill0)
  {}
};

struct common_c0b096
{
  arr<int> kodebr;

  common_c0b096() :
    kodebr(dimension(3000), fem::fill0)
  {}
};

struct common_c0b097
{
  arr<double> cki;

  common_c0b097() :
    cki(dimension(3000), fem::fill0)
  {}
};

struct common_c0b098
{
  arr<double> ckkjm;

  common_c0b098() :
    ckkjm(dimension(3000), fem::fill0)
  {}
};

struct common_c0b099
{
  arr<int> indhst;

  common_c0b099() :
    indhst(dimension(3000), fem::fill0)
  {}
};

struct common_c0b100
{
  arr<int> kodsem;

  common_c0b100() :
    kodsem(dimension(3000), fem::fill0)
  {}
};

struct common_c0b101
{
  arr<int> namebr;

  common_c0b101() :
    namebr(dimension(18000), fem::fill0)
  {}
};

struct common_c0b102
{
  arr<int> iform;

  common_c0b102() :
    iform(dimension(100), fem::fill0)
  {}
};

struct common_c0b103
{
  arr<int> node;

  common_c0b103() :
    node(dimension(100), fem::fill0)
  {}
};

struct common_c0b104
{
  arr<double> crest;

  common_c0b104() :
    crest(dimension(100), fem::fill0)
  {}
};

struct common_c0b105
{
  arr<double> time1;

  common_c0b105() :
    time1(dimension(100), fem::fill0)
  {}
};

struct common_c0b106
{
  arr<double> time2;

  common_c0b106() :
    time2(dimension(100), fem::fill0)
  {}
};

struct common_c0b107
{
  arr<double> tstart;

  common_c0b107() :
    tstart(dimension(100), fem::fill0)
  {}
};

struct common_c0b108
{
  arr<double> sfreq;

  common_c0b108() :
    sfreq(dimension(100), fem::fill0)
  {}
};

struct common_c0b109
{
  arr<int> kmswit;

  common_c0b109() :
    kmswit(dimension(3600), fem::fill0)
  {}
};

struct common_c0b110
{
  arr<int> nextsw;

  common_c0b110() :
    nextsw(dimension(1200), fem::fill0)
  {}
};

struct common_c0b111
{
  arr<double> rmfd;

  common_c0b111() :
    rmfd(dimension(1), fem::fill0)
  {}
};

struct common_c0b112
{
  arr<double> cikfd;

  common_c0b112() :
    cikfd(dimension(1), fem::fill0)
  {}
};

struct common_c0b113
{
  arr<int> imfd;

  common_c0b113() :
    imfd(dimension(600), fem::fill0)
  {}
};

struct common_c0b114
{
  arr<double> tclose;

  common_c0b114() :
    tclose(dimension(1200), fem::fill0)
  {}
};

struct common_c0b115
{
  arr<double> adelay;

  common_c0b115() :
    adelay(dimension(3600), fem::fill0)
  {}
};

struct common_c0b116
{
  arr<int> kpos;

  common_c0b116() :
    kpos(dimension(1200), fem::fill0)
  {}
};

struct common_c0b117
{
  arr<int> namesw;

  common_c0b117() :
    namesw(dimension(1200), fem::fill0)
  {}
};

struct common_c0b118
{
  arr<double> e;

  common_c0b118() :
    e(dimension(3002), fem::fill0)
  {}
};

struct common_c0b119
{
  arr<double> f;

  common_c0b119() :
    f(dimension(3002), fem::fill0)
  {}
};

struct common_c0b120
{
  arr<int> kssfrq;

  common_c0b120() :
    kssfrq(dimension(3002), fem::fill0)
  {}
};

struct common_c0b121
{
  arr<int> kode;

  common_c0b121() :
    kode(dimension(3002), fem::fill0)
  {}
};

struct common_c0b122
{
  arr<int> kpsour;

  common_c0b122() :
    kpsour(dimension(3002), fem::fill0)
  {}
};

struct common_c0b123
{
  vectorEx<double> volti;

  common_c0b123() :
    volti((6000), fem::fill0)
  {}
};

struct common_c0b124
{
  arr<double> voltk;

  common_c0b124() :
    voltk(dimension(3000), fem::fill0)
  {}
};

struct common_c0b125
{
  vectorEx<double> volt;

  common_c0b125() :
    volt((6000), fem::fill0)
  {}
};

struct common_c0b126
{
  arr<fem::str<8> > bus;

  common_c0b126() :
    bus(dimension(3002), fem::fill0)
  {}
};

struct common_smtacs
{
  arr<double> etac;
  arr<int> ismtac;
  int ntotac;
  int lbstac;

  common_smtacs() :
    etac(dimension(20), fem::fill0),
    ismtac(dimension(20), fem::fill0),
    ntotac(fem::int0),
    lbstac(fem::int0)
  {}
};

struct common_comlock
{
  arr<fem::str<8> > locker;

  common_comlock() :
    locker(dimension(2), fem::fill0)
  {}
};

struct common_smach
{
  arr<double> z;
  arr<double> x1;
  arr<double> smoutv;
  double sqrt3;
  double asqrt3;
  double sqrt32;
  double thtw;
  double athtw;
  double radeg;
  double omdt;
  double factom;
  double damrat;
  double delta6;
  double om2;
  double bin2;
  double bdam;
  int mfirst;
  int nst;
  int itold;
  int ibrold;
  int nsmout;
  int msmout;

  common_smach() :
    z(dimension(100), fem::fill0),
    x1(dimension(36), fem::fill0),
    smoutv(dimension(15), fem::fill0),
    sqrt3(fem::double0),
    asqrt3(fem::double0),
    sqrt32(fem::double0),
    thtw(fem::double0),
    athtw(fem::double0),
    radeg(fem::double0),
    omdt(fem::double0),
    factom(fem::double0),
    damrat(fem::double0),
    delta6(fem::double0),
    om2(fem::double0),
    bin2(fem::double0),
    bdam(fem::double0),
    mfirst(fem::int0),
    nst(fem::int0),
    itold(fem::int0),
    ibrold(fem::int0),
    nsmout(fem::int0),
    msmout(fem::int0)
  {}
};

struct common_umcom
{
  arr<fem::str<8> > busum;
  arr<double, 2> ptheta;
  arr<double, 2> zthevr;
  arr<double> vinp;
  arr<double> zthevs;
  arr<double> umcur;
  arr<double> con;
  arr<double> dumvec;
  arr<double, 2> dummat;
  arr<double> date;
  arr<double> clock;
  double sroot2;
  double sroot3;
  double omegrf;
  int inpu;
  int numbus;
  int ncltot;
  arr<int> ndum;
  int initum;
  int iureac;
  int iugpar;
  int iufpar;
  int iuhist;
  int iuumrp;
  int iunod1;
  int iunod2;
  int iujclt;
  int iujclo;
  int iujtyp;
  int iunodo;
  int iujtmt;
  int iuhism;
  int iuomgm;
  int iuomld;
  int iutham;
  int iuredu;
  int iureds;
  int iuflds;
  int iufldr;
  int iurequ;
  int iuflqs;
  int iuflqr;
  int iujcds;
  int iujcqs;
  int iuflxd;
  int iuflxq;
  int iunppa;
  int iurotm;
  int iuncld;
  int iunclq;
  int iujtqo;
  int iujomo;
  int iujtho;
  int iureqs;
  int iuepso;
  int iudcoe;
  int iukcoi;
  int iuvolt;
  int iuangl;
  int iunodf;
  int iunodm;
  int iukumo;
  int iujumo;
  int iuumou;
  int nclfix;
  int numfix;
  int iotfix;
  int ibsfix;
  int ksubum;
  int nsmach;
  int istart;

  common_umcom() :
    busum(dimension(50), fem::fill0),
    ptheta(dimension(3, 3), fem::fill0),
    zthevr(dimension(3, 3), fem::fill0),
    vinp(dimension(40), fem::fill0),
    zthevs(dimension(40), fem::fill0),
    umcur(dimension(40), fem::fill0),
    con(dimension(10), fem::fill0),
    dumvec(dimension(40), fem::fill0),
    dummat(dimension(3, 3), fem::fill0),
    date(dimension(2), fem::fill0),
    clock(dimension(2), fem::fill0),
    sroot2(fem::double0),
    sroot3(fem::double0),
    omegrf(fem::double0),
    inpu(fem::int0),
    numbus(fem::int0),
    ncltot(fem::int0),
    ndum(dimension(40), fem::fill0),
    initum(fem::int0),
    iureac(fem::int0),
    iugpar(fem::int0),
    iufpar(fem::int0),
    iuhist(fem::int0),
    iuumrp(fem::int0),
    iunod1(fem::int0),
    iunod2(fem::int0),
    iujclt(fem::int0),
    iujclo(fem::int0),
    iujtyp(fem::int0),
    iunodo(fem::int0),
    iujtmt(fem::int0),
    iuhism(fem::int0),
    iuomgm(fem::int0),
    iuomld(fem::int0),
    iutham(fem::int0),
    iuredu(fem::int0),
    iureds(fem::int0),
    iuflds(fem::int0),
    iufldr(fem::int0),
    iurequ(fem::int0),
    iuflqs(fem::int0),
    iuflqr(fem::int0),
    iujcds(fem::int0),
    iujcqs(fem::int0),
    iuflxd(fem::int0),
    iuflxq(fem::int0),
    iunppa(fem::int0),
    iurotm(fem::int0),
    iuncld(fem::int0),
    iunclq(fem::int0),
    iujtqo(fem::int0),
    iujomo(fem::int0),
    iujtho(fem::int0),
    iureqs(fem::int0),
    iuepso(fem::int0),
    iudcoe(fem::int0),
    iukcoi(fem::int0),
    iuvolt(fem::int0),
    iuangl(fem::int0),
    iunodf(fem::int0),
    iunodm(fem::int0),
    iukumo(fem::int0),
    iujumo(fem::int0),
    iuumou(fem::int0),
    nclfix(fem::int0),
    numfix(fem::int0),
    iotfix(fem::int0),
    ibsfix(fem::int0),
    ksubum(fem::int0),
    nsmach(fem::int0),
    istart(fem::int0)
  {}
};

struct common_spycom
{
  arr<double> rampcn;
  arr<double> rampsl;
  arr<int> kyramp;
  arr<double> fendrp;
  double tminrp;
  double tmaxrp;
  arr<double> tbegrp;
  arr<double> tendrp;
  arr<double> fbegrp;
  double tbreak;
  arr<double> epskon;

  common_spycom() :
    rampcn(dimension(20), fem::fill0),
    rampsl(dimension(20), fem::fill0),
    kyramp(dimension(20), fem::fill0),
    fendrp(dimension(20), fem::fill0),
    tminrp(fem::double0),
    tmaxrp(fem::double0),
    tbegrp(dimension(20), fem::fill0),
    tendrp(dimension(20), fem::fill0),
    fbegrp(dimension(20), fem::fill0),
    tbreak(fem::double0),
    epskon(dimension(14), fem::fill0)
  {}
};

struct common_spykom
{
  arr<int> indxrp;
  arr<int> ivec;
  arr<int> iascii;
  int numsym;
  int jjroll;
  int itexp;
  arr<int> labels;
  int maxarg;
  int kilper;
  int kfile5;
  int kverfy;
  int jword;
  int ibegcl;
  int iendcl;
  int lidnt1;
  int lidnt2;
  int nbreak;
  int linnow;
  int linspn;
  int numcrd;
  int munit5;
  int numkey;
  int indbuf;
  int indbeg;
  int mflush;
  int newvec;
  int maxflg;
  int kspsav;
  int memkar;
  int noback;
  arr<int> ksmspy;
  int lserlc;
  int kserlc;
  int kbrser;
  int lockbr;
  int kerase;
  int komadd;
  int iprspy;
  int monitr;
  int monits;
  arr<int> locate;
  arr<int> nline;
  int kbreak;
  int limbuf;
  int kolout;
  arr<int> limarr;
  arr<int> imin;
  arr<int> imax;
  int numex;
  arr<int> locout;
  arr<int> intout;
  int nexmod;
  int nextsn;
  int inchlp;
  int ksymbl;
  int kopyit;
  int kslowr;
  int limcrd;
  arr<int> looprp;
  arr<int> n10rmp;
  arr<int> memrmp;
  arr<int> kontac;
  arr<int> konadd;
  arr<int> kbegtx;
  arr<int> kar1;
  arr<int> kar2;
  int numrmp;
  int luntsp;
  bool logvar;

  common_spykom() :
    indxrp(dimension(20), fem::fill0),
    ivec(dimension(1000), fem::fill0),
    iascii(dimension(1000), fem::fill0),
    numsym(fem::int0),
    jjroll(fem::int0),
    itexp(fem::int0),
    labels(dimension(15), fem::fill0),
    maxarg(fem::int0),
    kilper(fem::int0),
    kfile5(fem::int0),
    kverfy(fem::int0),
    jword(fem::int0),
    ibegcl(fem::int0),
    iendcl(fem::int0),
    lidnt1(fem::int0),
    lidnt2(fem::int0),
    nbreak(fem::int0),
    linnow(fem::int0),
    linspn(fem::int0),
    numcrd(fem::int0),
    munit5(fem::int0),
    numkey(fem::int0),
    indbuf(fem::int0),
    indbeg(fem::int0),
    mflush(fem::int0),
    newvec(fem::int0),
    maxflg(fem::int0),
    kspsav(fem::int0),
    memkar(fem::int0),
    noback(fem::int0),
    ksmspy(dimension(3), fem::fill0),
    lserlc(fem::int0),
    kserlc(fem::int0),
    kbrser(fem::int0),
    lockbr(fem::int0),
    kerase(fem::int0),
    komadd(fem::int0),
    iprspy(fem::int0),
    monitr(fem::int0),
    monits(fem::int0),
    locate(dimension(1000), fem::fill0),
    nline(dimension(1000), fem::fill0),
    kbreak(fem::int0),
    limbuf(fem::int0),
    kolout(fem::int0),
    limarr(dimension(4), fem::fill0),
    imin(dimension(55), fem::fill0),
    imax(dimension(55), fem::fill0),
    numex(fem::int0),
    locout(dimension(55), fem::fill0),
    intout(dimension(55), fem::fill0),
    nexmod(fem::int0),
    nextsn(fem::int0),
    inchlp(fem::int0),
    ksymbl(fem::int0),
    kopyit(fem::int0),
    kslowr(fem::int0),
    limcrd(fem::int0),
    looprp(dimension(20), fem::fill0),
    n10rmp(dimension(20), fem::fill0),
    memrmp(dimension(20), fem::fill0),
    kontac(dimension(14), fem::fill0),
    konadd(dimension(14), fem::fill0),
    kbegtx(dimension(85), fem::fill0),
    kar1(dimension(1), fem::fill0),
    kar2(dimension(2), fem::fill0),
    numrmp(fem::int0),
    luntsp(fem::int0),
    logvar(fem::bool0)
  {}
};

struct common_spyf77
{
  arr<fem::str<1> > filext;
  arr<fem::str<8> > symb;
  arr<fem::str<1> > col;
  fem::str<20> bytfnd;
  fem::str<1> char1;
  arr<fem::str<8> > symbrp;
  fem::str<80> abufsv;
  fem::str<8> junker;
  fem::str<20> bytbuf;
  fem::str<80> buff77;
  arr<fem::str<80> > file6b;
  arr<fem::str<80> > file6;
  fem::str<80> blan80;
  fem::str<80> prom80;
  arr<fem::str<1> > digit;
  arr<fem::str<8> > texpar;
  arr<fem::str<8> > spykwd;
  fem::str<8> ansi8;
  fem::str<16> ansi16;
  fem::str<32> ansi32;
  fem::str<35> spycd2;
  fem::str<80> answ80;
  fem::str<8> brobus;
  fem::str<132> munit6;
  fem::str<132> outlin;
  fem::str<132> outsav;
  fem::str<132> heding;
  arr<fem::str<80> > texspy;

  common_spyf77() :
    filext(dimension(10), fem::fill0),
    symb(dimension(1000), fem::fill0),
    col(dimension(25), fem::fill0),
    bytfnd(fem::char0),
    char1(fem::char0),
    symbrp(dimension(20), fem::fill0),
    abufsv(fem::char0),
    junker(fem::char0),
    bytbuf(fem::char0),
    buff77(fem::char0),
    file6b(dimension(20), fem::fill0),
    file6(dimension(30000), fem::fill0),
    blan80(fem::char0),
    prom80(fem::char0),
    digit(dimension(10), fem::fill0),
    texpar(dimension(10), fem::fill0),
    spykwd(dimension(75), fem::fill0),
    ansi8(fem::char0),
    ansi16(fem::char0),
    ansi32(fem::char0),
    spycd2(fem::char0),
    answ80(fem::char0),
    brobus(fem::char0),
    munit6(fem::char0),
    outlin(fem::char0),
    outsav(fem::char0),
    heding(fem::char0),
    texspy(dimension(1250), fem::fill0)
  {}
};

struct common_comkwt
{
  int kwtvax;

  common_comkwt() :
    kwtvax(fem::int0)
  {}
};

struct common_cblock
{
  arr<double> datepl;
  arr<double> tclopl;
  arr<double> bbus;
  double tmult;
  double dy;
  double dx;
  double hpi;
  double tstep;
  double gxmin;
  double gxmax;
  arr<double> ew;
  double finfin;
  double fill;
  arr<double> fvcom;
  arr<double> yymin;
  arr<double> yymax;
  arr<double> ttmin;
  arr<double> ttmax;
  arr<double> ylevel;
  arr<double> ttlev;
  arr<double> dyold;
  arr<int> mlevel;
  arr<double> aaa;
  arr<double> bbb;
  arr<int> kp;
  arr<double> fxref;
  arr<double> fyref;
  double evnbyt;
  arr<double> ev;
  arr<double> bx;
  double vminr;
  double vmaxr;
  arr<int> mmm;
  arr<int> mstart;
  arr<int> numpts;
  int killpl;
  arr<int> kstart;
  arr<int> mplot;
  int jhmsp;
  int jchan;
  arr<int> labrtm;
  int jplt;
  int icp;
  int icurse;
  int mxypl;
  int indexp;
  int ind1;
  int numflt;
  int ncut;
  int numtek;
  int newfil;
  int mu6sav;
  arr<int> mcurve;
  int namvar;
  int mfake;
  int numraw;
  int nchsup;
  int nchver;
  int maxev;
  int kptplt;
  int numnvz;
  int nvz;
  int ncz;
  int numbrn;
  int numouz;
  int jplt1;
  int jbegbv;
  int jbegbc;
  int limfix;
  int nt2;
  int maxew;
  int maxip;
  arr<int> msymbt;
  int l4plot;
  arr<int> ivcom;

  common_cblock() :
    datepl(dimension(2), fem::fill0),
    tclopl(dimension(2), fem::fill0),
    bbus(dimension(300), fem::fill0),
    tmult(fem::double0),
    dy(fem::double0),
    dx(fem::double0),
    hpi(fem::double0),
    tstep(fem::double0),
    gxmin(fem::double0),
    gxmax(fem::double0),
    ew(dimension(15000), fem::fill0),
    finfin(fem::double0),
    fill(fem::double0),
    fvcom(dimension(50), fem::fill0),
    yymin(dimension(20), fem::fill0),
    yymax(dimension(20), fem::fill0),
    ttmin(dimension(20), fem::fill0),
    ttmax(dimension(20), fem::fill0),
    ylevel(dimension(20), fem::fill0),
    ttlev(dimension(20), fem::fill0),
    dyold(dimension(20), fem::fill0),
    mlevel(dimension(20), fem::fill0),
    aaa(dimension(20), fem::fill0),
    bbb(dimension(20), fem::fill0),
    kp(dimension(20), fem::fill0),
    fxref(dimension(25), fem::fill0),
    fyref(dimension(25), fem::fill0),
    evnbyt(fem::double0),
    ev(dimension(15000), fem::fill0),
    bx(dimension(150), fem::fill0),
    vminr(fem::double0),
    vmaxr(fem::double0),
    mmm(dimension(20), fem::fill0),
    mstart(dimension(20), fem::fill0),
    numpts(dimension(20), fem::fill0),
    killpl(fem::int0),
    kstart(dimension(20), fem::fill0),
    mplot(dimension(20), fem::fill0),
    jhmsp(fem::int0),
    jchan(fem::int0),
    labrtm(dimension(20), fem::fill0),
    jplt(fem::int0),
    icp(fem::int0),
    icurse(fem::int0),
    mxypl(fem::int0),
    indexp(fem::int0),
    ind1(fem::int0),
    numflt(fem::int0),
    ncut(fem::int0),
    numtek(fem::int0),
    newfil(fem::int0),
    mu6sav(fem::int0),
    mcurve(dimension(20), fem::fill0),
    namvar(fem::int0),
    mfake(fem::int0),
    numraw(fem::int0),
    nchsup(fem::int0),
    nchver(fem::int0),
    maxev(fem::int0),
    kptplt(fem::int0),
    numnvz(fem::int0),
    nvz(fem::int0),
    ncz(fem::int0),
    numbrn(fem::int0),
    numouz(fem::int0),
    jplt1(fem::int0),
    jbegbv(fem::int0),
    jbegbc(fem::int0),
    limfix(fem::int0),
    nt2(fem::int0),
    maxew(fem::int0),
    maxip(fem::int0),
    msymbt(dimension(20), fem::fill0),
    l4plot(fem::int0),
    ivcom(dimension(60), fem::fill0)
  {}
};

struct common_pltans
{
  arr<fem::str<8> > abuf77;
  fem::str<8> ansi;
  arr<fem::str<8> > ibuff;
  fem::str<8> texfnt;
  arr<fem::str<80> > sext;
  fem::str<80> headl;
  fem::str<80> vertl;
  fem::str<80> buffin;
  arr<fem::str<8> > slot1;
  arr<fem::str<24> > horzl;
  fem::str<8> date;
  fem::str<8> time;
  fem::str<8> textd1;
  fem::str<8> textd2;
  fem::str<8> curren;
  fem::str<8> voltag;
  fem::str<8> brclas;
  fem::str<80> filnam;
  fem::str<80> alpha;
  fem::str<24> xytitl;
  arr<fem::str<8> > anplt;

  common_pltans() :
    abuf77(dimension(10), fem::fill0),
    ansi(fem::char0),
    ibuff(dimension(20), fem::fill0),
    texfnt(fem::char0),
    sext(dimension(6), fem::fill0),
    headl(fem::char0),
    vertl(fem::char0),
    buffin(fem::char0),
    slot1(dimension(20), fem::fill0),
    horzl(dimension(8), fem::fill0),
    date(fem::char0),
    time(fem::char0),
    textd1(fem::char0),
    textd2(fem::char0),
    curren(fem::char0),
    voltag(fem::char0),
    brclas(fem::char0),
    filnam(fem::char0),
    alpha(fem::char0),
    xytitl(fem::char0),
    anplt(dimension(60), fem::fill0)
  {}
};

struct common_ekcom1
{
  arr<double, 2> ekbuf;
  arr<double, 2> ektemp;
  double errchk;
  arr<double> solrsv;
  arr<double> solisv;
  int nitera;
  int nekreq;
  arr<int> nekcod;

  common_ekcom1() :
    ekbuf(dimension(15, 9), fem::fill0),
    ektemp(dimension(45, 5), fem::fill0),
    errchk(fem::double0),
    solrsv(dimension(2500), fem::fill0),
    solisv(dimension(2500), fem::fill0),
    nitera(fem::int0),
    nekreq(fem::int0),
    nekcod(dimension(15), fem::fill0)
  {}
};

struct common_linemodel
{
  int kexact;
  int nsolve;
  double fminsv;
  int numrun;
  int nphlmt;
  fem::str<80> char80;
  arr<fem::str<6> > chlmfs;

  common_linemodel() :
    kexact(fem::int0),
    nsolve(fem::int0),
    fminsv(fem::double0),
    numrun(fem::int0),
    nphlmt(fem::int0),
    char80(fem::char0),
    chlmfs(dimension(18), fem::fill0)
  {}
};

struct common_systematic
{
  int linsys;

  common_systematic() :
    linsys(fem::int0)
  {}
};

struct common_komthl
{
  double pekexp;

  common_komthl() :
    pekexp(fem::double0)
  {}
};

struct common_com2
{
  int n1;
  int n2;
  int n3;
  int n4;
  int lcount;
  int model;
  int l27dep;
  int ibr1;
  int nrecur;
  int kgroup;
  int nc4;
  int nc5;
  int ifq;
  int n13;
  int ida;
  int ifkc;
  int idy;
  int idm;
  int idq;
  int idu;
  int idt;
  int iq;
  int nc6;
  int nc3;

  common_com2() :
    n1(fem::int0),
    n2(fem::int0),
    n3(fem::int0),
    n4(fem::int0),
    lcount(fem::int0),
    model(fem::int0),
    l27dep(fem::int0),
    ibr1(fem::int0),
    nrecur(fem::int0),
    kgroup(fem::int0),
    nc4(fem::int0),
    nc5(fem::int0),
    ifq(fem::int0),
    n13(fem::int0),
    ida(fem::int0),
    ifkc(fem::int0),
    idy(fem::int0),
    idm(fem::int0),
    idq(fem::int0),
    idu(fem::int0),
    idt(fem::int0),
    iq(fem::int0),
    nc6(fem::int0),
    nc3(fem::int0)
  {}
};

struct common_veccom
{
  int kntvec;
  arr<int> kofvec;

  common_veccom() :
    kntvec(fem::int0),
    kofvec(dimension(20), fem::fill0)
  {}
};

struct common_spac01
{
  arr<double> tp;

  common_spac01() :
    tp(dimension(30000), fem::fill0)
  {}
};

struct common_spac02
{
  arr<int> norder;

  common_spac02() :
    norder(dimension(3002), fem::fill0)
  {}
};

struct common_spac03
{
  arr<int> index;

  common_spac03() :
    index(dimension(3002), fem::fill0)
  {}
};

struct common_spac04
{
  arr<double> diag;

  common_spac04() :
    diag(dimension(3002), fem::fill0)
  {}
};

struct common_spac05
{
  arr<double> diab;

  common_spac05() :
    diab(dimension(3002), fem::fill0)
  {}
};

struct common_spac06
{
  arr<double> solr;

  common_spac06() :
    solr(dimension(3002), fem::fill0)
  {}
};

struct common_spac07
{
  arr<double> soli;

  common_spac07() :
    soli(dimension(3002), fem::fill0)
  {}
};

struct common_spac08
{
  vectorEx<int> ich1;

  common_spac08() :
    ich1((3002), fem::fill0)
  {}
};

const int sizeBND = 30000;  // was 300
struct common_spac09
{
  arr<double> bnd;

  common_spac09() :
    bnd(dimension(sizeBND), fem::fill0)
  {}
};

struct common_spac10
{
  arr<int> iloc;

  common_spac10() :
    iloc(dimension(30000), fem::fill0)
  {}
};

struct common_spac11
{
  arr<double> gnd;

  common_spac11() :
    gnd(dimension(30000), fem::fill0)
  {}
};

struct common_c10b01
{
  arr<int> jndex;

  common_c10b01() :
    jndex(dimension(1), fem::fill0)
  {}
};

struct common_c10b02
{
  arr<double> diagg;

  common_c10b02() :
    diagg(dimension(1), fem::fill0)
  {}
};

struct common_c10b03
{
  arr<double> diabb;

  common_c10b03() :
    diabb(dimension(1), fem::fill0)
  {}
};

struct common_c10b04
{
  arr<double> solrsv;

  common_c10b04() :
    solrsv(dimension(1), fem::fill0)
  {}
};

struct common_c10b05
{
  arr<double> solisv;

  common_c10b05() :
    solisv(dimension(1), fem::fill0)
  {}
};

struct common_c10b06
{
  arr<double> gndd;

  common_c10b06() :
    gndd(dimension(1), fem::fill0)
  {}
};

struct common_c10b07
{
  arr<double> bndd;

  common_c10b07() :
    bndd(dimension(1), fem::fill0)
  {}
};

struct common_c10b08
{
  arr<int> nekfix;

  common_c10b08() :
    nekfix(dimension(1), fem::fill0)
  {}
};

struct common_c10b09
{
  arr<fem::str<8> > fxtem1;

  common_c10b09() :
    fxtem1(dimension(1), fem::fill0)
  {}
};

struct common_c10b10
{
  arr<double> fxtem2;

  common_c10b10() :
    fxtem2(dimension(1), fem::fill0)
  {}
};

struct common_c10b11
{
  arr<double> fxtem3;

  common_c10b11() :
    fxtem3(dimension(1), fem::fill0)
  {}
};

struct common_c10b12
{
  arr<double> fxtem4;

  common_c10b12() :
    fxtem4(dimension(1), fem::fill0)
  {}
};

struct common_c10b13
{
  arr<double> fxtem5;

  common_c10b13() :
    fxtem5(dimension(1), fem::fill0)
  {}
};

struct common_c10b14
{
  arr<double> fxtem6;

  common_c10b14() :
    fxtem6(dimension(1), fem::fill0)
  {}
};

struct common_c10b15
{
  arr<fem::str<8> > fixbu1;

  common_c10b15() :
    fixbu1(dimension(1), fem::fill0)
  {}
};

struct common_c10b16
{
  arr<fem::str<8> > fixbu2;

  common_c10b16() :
    fixbu2(dimension(1), fem::fill0)
  {}
};

struct common_c10b17
{
  arr<fem::str<8> > fixbu3;

  common_c10b17() :
    fixbu3(dimension(1), fem::fill0)
  {}
};

struct common_c10b18
{
  arr<double> fixbu4;

  common_c10b18() :
    fixbu4(dimension(1), fem::fill0)
  {}
};

struct common_c10b19
{
  arr<double> fixbu5;

  common_c10b19() :
    fixbu5(dimension(1), fem::fill0)
  {}
};

struct common_c10b20
{
  arr<double> fixbu6;

  common_c10b20() :
    fixbu6(dimension(1), fem::fill0)
  {}
};

struct common_c10b21
{
  arr<double> fixbu7;

  common_c10b21() :
    fixbu7(dimension(1), fem::fill0)
  {}
};

struct common_c10b22
{
  arr<double> fixbu8;

  common_c10b22() :
    fixbu8(dimension(1), fem::fill0)
  {}
};

struct common_c10b23
{
  arr<double> fixbu9;

  common_c10b23() :
    fixbu9(dimension(1), fem::fill0)
  {}
};

struct common_c10b24
{
  arr<double> fixb10;

  common_c10b24() :
    fixb10(dimension(1), fem::fill0)
  {}
};

struct common_c10b25
{
  arr<double> fixb11;

  common_c10b25() :
    fixb11(dimension(1), fem::fill0)
  {}
};

struct common_c10b26
{
  arr<int> kndex;

  common_c10b26() :
    kndex(dimension(1), fem::fill0)
  {}
};

struct common_a8sw
{
  arr<double> a8sw;

  common_a8sw() :
    a8sw(dimension(400), fem::fill0)
  {}
};

struct common_fdqlcl
{
  int koff1;
  int koff2;
  int koff3;
  int koff4;
  int koff5;
  int koff6;
  int koff7;
  int koff8;
  int koff9;
  int koff10;
  int koff13;
  int koff14;
  int koff15;
  int koff16;
  int koff17;
  int koff18;
  int koff19;
  int koff20;
  int koff21;
  int koff22;
  int koff23;
  int koff24;
  int koff25;
  int inoff1;
  int inoff2;
  int inoff3;
  int inoff4;
  int inoff5;
  int nqtt;
  int lcbl;
  int lmode;
  int nqtw;

  common_fdqlcl() :
    koff1(fem::int0),
    koff2(fem::int0),
    koff3(fem::int0),
    koff4(fem::int0),
    koff5(fem::int0),
    koff6(fem::int0),
    koff7(fem::int0),
    koff8(fem::int0),
    koff9(fem::int0),
    koff10(fem::int0),
    koff13(fem::int0),
    koff14(fem::int0),
    koff15(fem::int0),
    koff16(fem::int0),
    koff17(fem::int0),
    koff18(fem::int0),
    koff19(fem::int0),
    koff20(fem::int0),
    koff21(fem::int0),
    koff22(fem::int0),
    koff23(fem::int0),
    koff24(fem::int0),
    koff25(fem::int0),
    inoff1(fem::int0),
    inoff2(fem::int0),
    inoff3(fem::int0),
    inoff4(fem::int0),
    inoff5(fem::int0),
    nqtt(fem::int0),
    lcbl(fem::int0),
    lmode(fem::int0),
    nqtw(fem::int0)
  {}
};

struct common_com44
{
  arr<double> bcars;
  arr<double> ccars;
  arr<double> dcars;
  arr<double> fbe;
  arr<fem::str<8> > brname;
  arr<double> fbed;
  arr<double> fke;
  arr<double> fked;
  //double pi;
  double picon;
  double sqrt2;
  double valu1;
  double valu2;
  double valu3;
  double valu4;
  double valu5;
  double valu6;
  double valu7;
  double valu8;
  double valu9;
  double valu10;
  double valu11;
  double valu12;
  double valu13;
  double corchk;
  double aaa1;
  double aaa2;
  int ll0;
  int ll1;
  int ll2;
  int ll3;
  int ll5;
  int ll6;
  int ll7;
  int ll8;
  int ll9;
  int ll10;
  int lphase;
  int lphpl1;
  int lphd2;
  int lgdbd;
  int jpralt;
  int nfreq;

  common_com44() :
    bcars(dimension(30), fem::fill0),
    ccars(dimension(30), fem::fill0),
    dcars(dimension(30), fem::fill0),
    fbe(dimension(20), fem::fill0),
    brname(dimension(40), fem::fill0),
    fbed(dimension(20), fem::fill0),
    fke(dimension(20), fem::fill0),
    fked(dimension(20), fem::fill0),
    //pi(fem::double0),
    picon(fem::double0),
    sqrt2(fem::double0),
    valu1(fem::double0),
    valu2(fem::double0),
    valu3(fem::double0),
    valu4(fem::double0),
    valu5(fem::double0),
    valu6(fem::double0),
    valu7(fem::double0),
    valu8(fem::double0),
    valu9(fem::double0),
    valu10(fem::double0),
    valu11(fem::double0),
    valu12(fem::double0),
    valu13(fem::double0),
    corchk(fem::double0),
    aaa1(fem::double0),
    aaa2(fem::double0),
    ll0(fem::int0),
    ll1(fem::int0),
    ll2(fem::int0),
    ll3(fem::int0),
    ll5(fem::int0),
    ll6(fem::int0),
    ll7(fem::int0),
    ll8(fem::int0),
    ll9(fem::int0),
    ll10(fem::int0),
    lphase(fem::int0),
    lphpl1(fem::int0),
    lphd2(fem::int0),
    lgdbd(fem::int0),
    jpralt(fem::int0),
    nfreq(fem::int0)
  {}
};

struct common_umlocal
{
  arr<fem::str<8> > texta;
  double d1;
  double d2;
  double d3;
  double d17;
  double stat59;
  double fmum;
  double rmvaum;
  double rkvum;
  double s1um;
  double s2um;
  double zlsbum;
  double s1qum;
  double s2qum;
  double aglqum;
  double raum;
  double xdum;
  double squm;
  double xdpum;
  double xqpum;
  double xdppum;
  double xqppum;
  double tdpum;
  double tdppum;
  double x0um;
  double rnum;
  double xnum;
  double xfum;
  double xdfum;
  double xdkdum;
  double xkdum;
  double xkqum;
  double xqkqum;
  double xgkqum;
  double xqum;
  double xqgum;
  double xgum;
  double distrf;
  double hjum;
  double dsynum;
  double dmutum;
  double spring;
  double dabsum;
  double tqppum;
  double agldum;
  double xlum;
  int nz1;
  int nz2;
  int nz3;
  int nz4;
  int n5;
  int n6;
  int n7;
  int n8;
  int n9;
  int n10;
  int n11;
  int n12;
  int n14;
  int n15;
  int n16;
  int n17;
  int n18;
  int n19;
  int n20;
  int jr;
  int jf;
  int nexc;
  int kconex;
  int ibrexc;
  int nstan;
  int numasu;
  int nmgen;
  int nmexc;
  int ntypsm;
  int netrun;
  int netrum;
  int nsmtpr;
  int nsmtac;
  int nrsyn;
  int ntorq;
  int mlum;
  int nparum;
  int ngroup;
  int nall;
  int nangre;
  int nexcsw;
  int limasu;
  int lopss2;
  int lopss1;
  int lopss8;
  int lopss9;
  int lopss10;
  int lopss4;
  int nshare;

  common_umlocal() :
    texta(dimension(101), fem::fill0),
    d1(fem::double0),
    d2(fem::double0),
    d3(fem::double0),
    d17(fem::double0),
    stat59(fem::double0),
    fmum(fem::double0),
    rmvaum(fem::double0),
    rkvum(fem::double0),
    s1um(fem::double0),
    s2um(fem::double0),
    zlsbum(fem::double0),
    s1qum(fem::double0),
    s2qum(fem::double0),
    aglqum(fem::double0),
    raum(fem::double0),
    xdum(fem::double0),
    squm(fem::double0),
    xdpum(fem::double0),
    xqpum(fem::double0),
    xdppum(fem::double0),
    xqppum(fem::double0),
    tdpum(fem::double0),
    tdppum(fem::double0),
    x0um(fem::double0),
    rnum(fem::double0),
    xnum(fem::double0),
    xfum(fem::double0),
    xdfum(fem::double0),
    xdkdum(fem::double0),
    xkdum(fem::double0),
    xkqum(fem::double0),
    xqkqum(fem::double0),
    xgkqum(fem::double0),
    xqum(fem::double0),
    xqgum(fem::double0),
    xgum(fem::double0),
    distrf(fem::double0),
    hjum(fem::double0),
    dsynum(fem::double0),
    dmutum(fem::double0),
    spring(fem::double0),
    dabsum(fem::double0),
    tqppum(fem::double0),
    agldum(fem::double0),
    xlum(fem::double0),
    nz1(fem::int0),
    nz2(fem::int0),
    nz3(fem::int0),
    nz4(fem::int0),
    n5(fem::int0),
    n6(fem::int0),
    n7(fem::int0),
    n8(fem::int0),
    n9(fem::int0),
    n10(fem::int0),
    n11(fem::int0),
    n12(fem::int0),
    n14(fem::int0),
    n15(fem::int0),
    n16(fem::int0),
    n17(fem::int0),
    n18(fem::int0),
    n19(fem::int0),
    n20(fem::int0),
    jr(fem::int0),
    jf(fem::int0),
    nexc(fem::int0),
    kconex(fem::int0),
    ibrexc(fem::int0),
    nstan(fem::int0),
    numasu(fem::int0),
    nmgen(fem::int0),
    nmexc(fem::int0),
    ntypsm(fem::int0),
    netrun(fem::int0),
    netrum(fem::int0),
    nsmtpr(fem::int0),
    nsmtac(fem::int0),
    nrsyn(fem::int0),
    ntorq(fem::int0),
    mlum(fem::int0),
    nparum(fem::int0),
    ngroup(fem::int0),
    nall(fem::int0),
    nangre(fem::int0),
    nexcsw(fem::int0),
    limasu(fem::int0),
    lopss2(fem::int0),
    lopss1(fem::int0),
    lopss8(fem::int0),
    lopss9(fem::int0),
    lopss10(fem::int0),
    lopss4(fem::int0),
    nshare(fem::int0)
  {}
};

struct common_umlocl
{
  int n1z;
  int n2z;
  int n3z;
  int n4z;
  int n5z;
  int n6z;
  int n7z;
  int n8z;
  int n9z;
  int n10z;
  int n11z;
  int n12z;
  int n17z;
  int n18z;
  int n19z;
  int n20z;
  double d1z;
  double d2z;
  double d3z;
  double d4;
  double d5;
  double d6;
  double d7;
  double d8;
  double d9;
  double d10;
  double d11;
  double d12;
  double d13;
  double d14;
  double d15;
  double d16;
  double d17z;
  double d18;
  int lfim3;
  int lfim4i;
  int ncomcl;
  int ncomum;
  int kcld1;
  int kclq1;
  int kclf;
  int nminum;
  int lopsz1;
  int lopsz2;
  int lopsz4;
  int lopsz8;
  int lopsz9;
  int lopsz10;
  double slip;

  common_umlocl() :
    n1z(fem::int0),
    n2z(fem::int0),
    n3z(fem::int0),
    n4z(fem::int0),
    n5z(fem::int0),
    n6z(fem::int0),
    n7z(fem::int0),
    n8z(fem::int0),
    n9z(fem::int0),
    n10z(fem::int0),
    n11z(fem::int0),
    n12z(fem::int0),
    n17z(fem::int0),
    n18z(fem::int0),
    n19z(fem::int0),
    n20z(fem::int0),
    d1z(fem::double0),
    d2z(fem::double0),
    d3z(fem::double0),
    d4(fem::double0),
    d5(fem::double0),
    d6(fem::double0),
    d7(fem::double0),
    d8(fem::double0),
    d9(fem::double0),
    d10(fem::double0),
    d11(fem::double0),
    d12(fem::double0),
    d13(fem::double0),
    d14(fem::double0),
    d15(fem::double0),
    d16(fem::double0),
    d17z(fem::double0),
    d18(fem::double0),
    lfim3(fem::int0),
    lfim4i(fem::int0),
    ncomcl(fem::int0),
    ncomum(fem::int0),
    kcld1(fem::int0),
    kclq1(fem::int0),
    kclf(fem::int0),
    nminum(fem::int0),
    lopsz1(fem::int0),
    lopsz2(fem::int0),
    lopsz4(fem::int0),
    lopsz8(fem::int0),
    lopsz9(fem::int0),
    lopsz10(fem::int0),
    slip(fem::double0)
  {}
};

struct common_com29
{
  double per;
  double xmean1;
  double xvar1;
  double stdev1;
  double vmax;
  int liminc;
  int iofarr;
  int nvar;
  int key;
  int maxo29;

  common_com29() :
    per(fem::double0),
    xmean1(fem::double0),
    xvar1(fem::double0),
    stdev1(fem::double0),
    vmax(fem::double0),
    liminc(fem::int0),
    iofarr(fem::int0),
    nvar(fem::int0),
    key(fem::int0),
    maxo29(fem::int0)
  {}
};

struct common_ldec31
{
  int kalcom;

  common_ldec31() :
    kalcom(fem::int0)
  {}
};

//struct common_c31b01
//{
//  arr<int> karray;
//
//  common_c31b01() :
//    karray(dimension(300), fem::fill0)
//  {}
//};

struct common_com39
{
  arr<double, 2> tir;
  arr<double, 2> tii;
  arr<double, 2> tdum;
  arr<int, 2> modskp;
  arr<double> alinvc;
  arr<double> akfrac;
  arr<double> alphaf;
  arr<double> fczr;
  arr<double> fcpr;
  arr<double> fcz;
  arr<double> fcp;
  arr<int> indxv;
  arr<double> xauxd;
  arr<double> zoprau;
  arr<double> zoprao;
  arr<double> azepo;
  arr<double> xchkra;
  arr<double> xknee;
  arr<int> noprao;
  double hreflg;
  double aptdec;
  double gmode;
  double amina1;
  double onehav;
  double oneqtr;
  double hrflgr;
  double epstol;
  double refa;
  double refb;
  int idebug;
  int iftype;
  int lout;
  int ndata;
  int ntotra;
  int nzone;
  int izone;
  int nrange;
  int modify;
  int nexmis;
  int normax;
  int ifwta;
  int koutpr;
  int inelim;
  int ifplot;
  int ifdat;
  int iecode;
  int nzeror;
  int npoler;
  int modesk;
  int metrik;

  common_com39() :
    tir(dimension(18, 18), fem::fill0),
    tii(dimension(18, 18), fem::fill0),
    tdum(dimension(18, 18), fem::fill0),
    modskp(dimension(2, 18), fem::fill0),
    alinvc(dimension(90), fem::fill0),
    akfrac(dimension(100), fem::fill0),
    alphaf(dimension(100), fem::fill0),
    fczr(dimension(100), fem::fill0),
    fcpr(dimension(100), fem::fill0),
    fcz(dimension(100), fem::fill0),
    fcp(dimension(100), fem::fill0),
    indxv(dimension(100), fem::fill0),
    xauxd(dimension(200), fem::fill0),
    zoprau(dimension(400), fem::fill0),
    zoprao(dimension(400), fem::fill0),
    azepo(dimension(400), fem::fill0),
    xchkra(dimension(255), fem::fill0),
    xknee(dimension(100), fem::fill0),
    noprao(dimension(100), fem::fill0),
    hreflg(fem::double0),
    aptdec(fem::double0),
    gmode(fem::double0),
    amina1(fem::double0),
    onehav(fem::double0),
    oneqtr(fem::double0),
    hrflgr(fem::double0),
    epstol(fem::double0),
    refa(fem::double0),
    refb(fem::double0),
    idebug(fem::int0),
    iftype(fem::int0),
    lout(fem::int0),
    ndata(fem::int0),
    ntotra(fem::int0),
    nzone(fem::int0),
    izone(fem::int0),
    nrange(fem::int0),
    modify(fem::int0),
    nexmis(fem::int0),
    normax(fem::int0),
    ifwta(fem::int0),
    koutpr(fem::int0),
    inelim(fem::int0),
    ifplot(fem::int0),
    ifdat(fem::int0),
    iecode(fem::int0),
    nzeror(fem::int0),
    npoler(fem::int0),
    modesk(fem::int0),
    metrik(fem::int0)
  {}
};

struct common_c39b01
{
  arr<double> xdat;

  common_c39b01() :
    xdat(dimension(10000), fem::fill0)
  {}
};

struct common_c39b02
{
  arr<double> ydat;

  common_c39b02() :
    ydat(dimension(10000), fem::fill0)
  {}
};

struct common_c39b03
{
  arr<double> aphdat;

  common_c39b03() :
    aphdat(dimension(10000), fem::fill0)
  {}
};

struct common_c44b02
{
  arr<double> p;

  common_c44b02() :
    p(dimension(77815), fem::fill0)
  {}
};

struct common_c44b03
{
  arr<double> z;

  common_c44b03() :
    z(dimension(77815), fem::fill0)
  {}
};

struct common_c44b04
{
  arr<int> ic;

  common_c44b04() :
    ic(dimension(394), fem::fill0)
  {}
};

struct common_c44b05
{
  arr<double> r;

  common_c44b05() :
    r(dimension(394), fem::fill0)
  {}
};

struct common_c44b06
{
  arr<double> dz;

  common_c44b06() :
    dz(dimension(394), fem::fill0)
  {}
};

struct common_c44b07
{
  arr<double> gmd;

  common_c44b07() :
    gmd(dimension(394), fem::fill0)
  {}
};

struct common_c44b08
{
  arr<double> x;

  common_c44b08() :
    x(dimension(394), fem::fill0)
  {}
};

struct common_c44b09
{
  arr<double> y;

  common_c44b09() :
    y(dimension(394), fem::fill0)
  {}
};

struct common_c44b10
{
  arr<double> tb2;

  common_c44b10() :
    tb2(dimension(394), fem::fill0)
  {}
};

struct common_c44b11
{
  arr<int> itb3;

  common_c44b11() :
    itb3(dimension(394), fem::fill0)
  {}
};

struct common_c44b12
{
  arr<double> workr1;

  common_c44b12() :
    workr1(dimension(394), fem::fill0)
  {}
};

struct common_c44b13
{
  arr<double> workr2;

  common_c44b13() :
    workr2(dimension(394), fem::fill0)
  {}
};

struct common_c44b14
{
  arr<fem::str<8> > text;

  common_c44b14() :
    text(dimension(788), fem::fill0)
  {}
};

struct common_c44b15
{
  arr<double> gd;

  common_c44b15() :
    gd(dimension(19503), fem::fill0)
  {}
};

struct common_c44b16
{
  arr<double> bd;

  common_c44b16() :
    bd(dimension(19503), fem::fill0)
  {}
};

struct common_c44b17
{
  arr<double> yd;

  common_c44b17() :
    yd(dimension(19503), fem::fill0)
  {}
};

struct common_c44b18
{
  arr<int> itbic;

  common_c44b18() :
    itbic(dimension(395), fem::fill0)
  {}
};

struct common_c44b19
{
  arr<double> tbr;

  common_c44b19() :
    tbr(dimension(395), fem::fill0)
  {}
};

struct common_c44b20
{
  arr<double> tbd;

  common_c44b20() :
    tbd(dimension(395), fem::fill0)
  {}
};

struct common_c44b21
{
  arr<double> tbg;

  common_c44b21() :
    tbg(dimension(395), fem::fill0)
  {}
};

struct common_c44b22
{
  arr<double> tbx;

  common_c44b22() :
    tbx(dimension(395), fem::fill0)
  {}
};

struct common_c44b23
{
  arr<double> tby;

  common_c44b23() :
    tby(dimension(395), fem::fill0)
  {}
};

struct common_c44b24
{
  arr<double> tbtb2;

  common_c44b24() :
    tbtb2(dimension(395), fem::fill0)
  {}
};

struct common_c44b25
{
  arr<int> itbtb3;

  common_c44b25() :
    itbtb3(dimension(395), fem::fill0)
  {}
};

struct common_c44b26
{
  arr<fem::str<8> > tbtext;

  common_c44b26() :
    tbtext(dimension(395), fem::fill0)
  {}
};

struct common_volpri
{
  arr<double> volti_50; //
  arr<double> voltk_50;
  arr<double> vim;

  common_volpri() :
    volti_50(dimension(50), fem::fill0),
    voltk_50(dimension(50), fem::fill0),
    vim(dimension(50), fem::fill0)
  {}
};

//struct common_c44b01
//{
//  arr<int> karray;
//
//  common_c44b01() :
//    karray(dimension(300), fem::fill0)
//  {}
//};

struct common_com45
{
  arr<fem::str<8> > pl;
  double f;
  double w;
  double cold;
  double xpan;
  double conv5;
  double ratio;
  double pi2;
  double sll;
  double spdlt;
  double tt;
  double tstrt;
  double tretrd;
  double tstep;
  double ffin;
  double shiftr;
  double shifti;
  double d;
  arr<double> x;
  double dplu;
  double dmin;
  int ictrl;
  int i1;
  int iwork;
  int nph;
  int nph2;
  int nphpi2;
  int n22;
  int nphsq;
  int ntri;
  int iss;
  int nfr;
  int nfr1;
  int ix;
  int kreqab;

  common_com45() :
    pl(dimension(91), fem::fill0),
    f(fem::double0),
    w(fem::double0),
    cold(fem::double0),
    xpan(fem::double0),
    conv5(fem::double0),
    ratio(fem::double0),
    pi2(fem::double0),
    sll(fem::double0),
    spdlt(fem::double0),
    tt(fem::double0),
    tstrt(fem::double0),
    tretrd(fem::double0),
    tstep(fem::double0),
    ffin(fem::double0),
    shiftr(fem::double0),
    shifti(fem::double0),
    d(fem::double0),
    x(dimension(3), fem::fill0),
    dplu(fem::double0),
    dmin(fem::double0),
    ictrl(fem::int0),
    i1(fem::int0),
    iwork(fem::int0),
    nph(fem::int0),
    nph2(fem::int0),
    nphpi2(fem::int0),
    n22(fem::int0),
    nphsq(fem::int0),
    ntri(fem::int0),
    iss(fem::int0),
    nfr(fem::int0),
    nfr1(fem::int0),
    ix(fem::int0),
    kreqab(fem::int0)
  {}
};

//struct common_c45b01
//{
//  arr<int> karray;
//
//  common_c45b01() :
//    karray(dimension(300), fem::fill0)
//  {}
//};

struct common_com47
{
  arr<std::complex<double> > bin;
  arr<std::complex<double> > bkn;
  std::complex<double> cimag1;
  std::complex<double> creal1;
  std::complex<double> czero;
  std::complex<double> ypo;
  double alf1;
  double alf2;
  double dep1;
  double dep2;
  double e0;
  double e2p;
  arr<double> radp;
  double alpi;
  double bp1;
  double bp2;
  double es1;
  double es2;
  double rop;
  double usp;
  double hyud2;
  double hyud3;
  double hyud4;
  double htoj2;
  double htoj3;
  double fzero;
  double htoj4;
  double pai;
  double roe;
  double spdlgt;
  double u0;
  double u2p;
  double value1;
  double value2;
  double value3;
  double value4;
  double value5;
  double valu14;
  int iearth;
  int itypec;
  int ncct;
  int ncc;
  int npc;
  int izflag;
  int iyflag;
  int npc2;
  int np2;
  int logsix;
  int kmode;
  int iprs47;
  int npais;
  int ncros;
  int numaki;
  int npp;
  int iprint;

  common_com47() :
    bin(dimension(20), fem::fill0),
    bkn(dimension(20), fem::fill0),
    cimag1(fem::double0),
    creal1(fem::double0),
    czero(fem::double0),
    ypo(fem::double0),
    alf1(fem::double0),
    alf2(fem::double0),
    dep1(fem::double0),
    dep2(fem::double0),
    e0(fem::double0),
    e2p(fem::double0),
    radp(dimension(3), fem::fill0),
    alpi(fem::double0),
    bp1(fem::double0),
    bp2(fem::double0),
    es1(fem::double0),
    es2(fem::double0),
    rop(fem::double0),
    usp(fem::double0),
    hyud2(fem::double0),
    hyud3(fem::double0),
    hyud4(fem::double0),
    htoj2(fem::double0),
    htoj3(fem::double0),
    fzero(fem::double0),
    htoj4(fem::double0),
    pai(fem::double0),
    roe(fem::double0),
    spdlgt(fem::double0),
    u0(fem::double0),
    u2p(fem::double0),
    value1(fem::double0),
    value2(fem::double0),
    value3(fem::double0),
    value4(fem::double0),
    value5(fem::double0),
    valu14(fem::double0),
    iearth(fem::int0),
    itypec(fem::int0),
    ncct(fem::int0),
    ncc(fem::int0),
    npc(fem::int0),
    izflag(fem::int0),
    iyflag(fem::int0),
    npc2(fem::int0),
    np2(fem::int0),
    logsix(fem::int0),
    kmode(fem::int0),
    iprs47(fem::int0),
    npais(fem::int0),
    ncros(fem::int0),
    numaki(fem::int0),
    npp(fem::int0),
    iprint(fem::int0)
  {}
};

//struct common_c47b01
//{
//  arr<int> karray;
//
//  common_c47b01() :
//    karray(dimension(300), fem::fill0)
//  {}
//};

struct common_newt1
{
  arr<double> rwin;
  double zhl;
  double zht;
  double zlt;
  int k;
  int m;
  int idelt;
  int logsix;

  common_newt1() :
    rwin(dimension(10), fem::fill0),
    zhl(fem::double0),
    zht(fem::double0),
    zlt(fem::double0),
    k(fem::int0),
    m(fem::int0),
    idelt(fem::int0),
    logsix(fem::int0)
  {}
};

struct common_zprint
{
  arr<double> zoutr;
  arr<double> zoutx;

  common_zprint() :
    zoutr(dimension(120), fem::fill0),
    zoutx(dimension(120), fem::fill0)
  {}
};

struct common :
  fem::common,
  common_cmn,
  common_comthl,
  common_comld,
  common_c29b01,
  common_c0b001,
  common_c0b002,
  common_c0b003,
  common_c0b004,
  common_c0b005,
  common_c0b006,
  common_c0b007,
  common_c0b008,
  common_c0b009,
  common_c0b010,
  common_c0b011,
  common_c0b012,
  common_c0b013,
  common_c0b014,
  common_c0b015,
  common_c0b016,
  common_c0b017,
  common_c0b018,
  common_c0b019,
  common_c0b020,
  common_c0b021,
  common_c0b022,
  common_c0b023,
  common_c0b024,
  common_c0b025,
  common_c0b026,
  common_c0b027,
  common_c0b028,
  common_c0b029,
  common_c0b030,
  common_c0b031,
  common_c0b032,
  common_c0b033,
  common_c0b034,
  common_c0b035,
  common_c0b036,
  common_c0b037,
  common_c0b038,
  common_c0b039,
  common_c0b040,
  common_c0b041,
  common_c0b042,
  common_c0b043,
  common_c0b044,
  common_c0b045,
  common_c0b046,
  common_c0b047,
  common_c0b048,
  common_c0b049,
  common_c0b050,
  common_c0b051,
  common_c0b052,
  common_c0b053,
  common_c0b054,
  common_c0b055,
  common_c0b056,
  common_c0b057,
  common_c0b058,
  common_c0b059,
  common_c0b060,
  common_c0b061,
  common_c0b062,
  common_c0b063,
  common_c0b064,
  common_c0b065,
  common_c0b066,
  common_c0b067,
  common_c0b068,
  common_c0b069,
  common_c0b070,
  common_c0b071,
  common_c0b072,
  common_c0b073,
  common_c0b074,
  common_c0b075,
  common_c0b076,
  common_c0b077,
  common_c0b078,
  common_c0b079,
  common_c0b080,
  common_c0b081,
  common_c0b082,
  common_c0b083,
  common_c0b084,
  common_c0b085,
  common_c0b086,
  common_c0b087,
  common_c0b088,
  common_c0b089,
  common_c0b090,
  common_c0b091,
  common_c0b092,
  common_c0b093,
  common_c0b094,
  common_c0b095,
  common_c0b096,
  common_c0b097,
  common_c0b098,
  common_c0b099,
  common_c0b100,
  common_c0b101,
  common_c0b102,
  common_c0b103,
  common_c0b104,
  common_c0b105,
  common_c0b106,
  common_c0b107,
  common_c0b108,
  common_c0b109,
  common_c0b110,
  common_c0b111,
  common_c0b112,
  common_c0b113,
  common_c0b114,
  common_c0b115,
  common_c0b116,
  common_c0b117,
  common_c0b118,
  common_c0b119,
  common_c0b120,
  common_c0b121,
  common_c0b122,
  common_c0b123,
  common_c0b124,
  common_c0b125,
  common_c0b126,
  common_smtacs,
  common_comlock,
  common_smach,
  common_umcom,
  common_spycom,
  common_spykom,
  common_spyf77,
  common_comkwt,
  common_cblock,
  common_pltans,
  common_ekcom1,
  common_linemodel,
  common_systematic,
  common_komthl,
  common_com2,
  common_veccom,
  common_spac01,
  common_spac02,
  common_spac03,
  common_spac04,
  common_spac05,
  common_spac06,
  common_spac07,
  common_spac08,
  common_spac09,
  common_spac10,
  common_spac11,
  common_c10b01,
  common_c10b02,
  common_c10b03,
  common_c10b04,
  common_c10b05,
  common_c10b06,
  common_c10b07,
  common_c10b08,
  common_c10b09,
  common_c10b10,
  common_c10b11,
  common_c10b12,
  common_c10b13,
  common_c10b14,
  common_c10b15,
  common_c10b16,
  common_c10b17,
  common_c10b18,
  common_c10b19,
  common_c10b20,
  common_c10b21,
  common_c10b22,
  common_c10b23,
  common_c10b24,
  common_c10b25,
  common_c10b26,
  common_a8sw,
  common_fdqlcl,
  common_com44,
  common_umlocal,
  common_umlocl,
  common_com29,
  common_ldec31,
  //common_c31b01,
  common_com39,
  common_c39b01,
  common_c39b02,
  common_c39b03,
  common_c44b02,
  common_c44b03,
  common_c44b04,
  common_c44b05,
  common_c44b06,
  common_c44b07,
  common_c44b08,
  common_c44b09,
  common_c44b10,
  common_c44b11,
  common_c44b12,
  common_c44b13,
  common_c44b14,
  common_c44b15,
  common_c44b16,
  common_c44b17,
  common_c44b18,
  common_c44b19,
  common_c44b20,
  common_c44b21,
  common_c44b22,
  common_c44b23,
  common_c44b24,
  common_c44b25,
  common_c44b26,
  common_volpri,
  //common_c44b01,
  common_com45,
  //common_c45b01,
  common_com47,
  //common_c47b01,
  common_newt1,
  common_zprint
{
  fem::cmn_sve tables_sve;
  fem::cmn_sve frenum_sve;
  fem::cmn_sve frefld_sve;
  fem::cmn_sve cimage_sve;
  fem::cmn_sve namea6_sve;
  fem::cmn_sve tacs1a_sve;
  fem::cmn_sve tacs1b_sve;
  fem::cmn_sve tacs1_sve;
  fem::cmn_sve sandnm_sve;
  fem::cmn_sve spyout_sve;
  fem::cmn_sve spyink_sve;
  fem::cmn_sve setrtm_sve;
  fem::cmn_sve chrplt_sve;
  fem::cmn_sve tpplot_sve;
  fem::cmn_sve spying_sve;
  fem::cmn_sve emtspy_sve;
  fem::cmn_sve initsp_sve;
  fem::cmn_sve datain_sve;
  fem::cmn_sve swmodf_sve;
  fem::cmn_sve expchk_sve;
  fem::cmn_sve reques_sve;
  fem::cmn_sve sysdep_sve;
  fem::cmn_sve over1_sve;
  fem::cmn_sve pltfil_sve;
  fem::cmn_sve smint_sve;
  fem::cmn_sve over11_sve;
  fem::cmn_sve over13_sve;
  fem::cmn_sve smout_sve;
  fem::cmn_sve over15_sve;
  fem::cmn_sve yserlc_sve;
  fem::cmn_sve subts1_sve;
  fem::cmn_sve zincox_sve;
  fem::cmn_sve analyt_sve;
  fem::cmn_sve inlmfs_sve;
  fem::cmn_sve nonln2_sve;
  fem::cmn_sve distr2_sve;
  fem::cmn_sve over3_sve;
  fem::cmn_sve over2_sve;
  fem::cmn_sve umdatb_sve;
  fem::cmn_sve umdata_sve;
  fem::cmn_sve smdat_sve;
  fem::cmn_sve over5a_sve;
  fem::cmn_sve over5_sve;
  fem::cmn_sve plotng_sve;
  fem::cmn_sve innr29_sve;
  fem::cmn_sve fltdat_sve;
  fem::cmn_sve statrs_sve;
  fem::cmn_sve guts29_sve;
  fem::cmn_sve axis_sve;
  fem::cmn_sve linplt_sve;
  fem::cmn_sve subr31_sve;
  fem::cmn_sve ftplot_sve;
  fem::cmn_sve misc39_sve;
  fem::cmn_sve subr39_sve;
  fem::cmn_sve unwind_sve;
  fem::cmn_sve modal_sve;
  fem::cmn_sve output_sve;
  fem::cmn_sve guts44_sve;
  fem::cmn_sve xift_sve;
  fem::cmn_sve tdfit_sve;
  fem::cmn_sve guts45_sve;
  fem::cmn_sve print_sve;
  fem::cmn_sve prcon_sve;
  fem::cmn_sve guts47_sve;
  fem::cmn_sve blockdata_unnamed_sve;
  fem::cmn_sve blockdata_blkplt_sve;
  fem::cmn_sve bctran_sve;
  fem::cmn_sve over41_sve;
  fem::cmn_sve hysdat_sve;
  fem::cmn_sve arrdat_sve;
  fem::cmn_sve subr51_sve;
  fem::cmn_sve subr55_sve;
  fem::cmn_sve program_main_sve;

  std::ifstream inp_stream;
  std::ofstream out_stream;
  std::ofstream out2_stream; // steady state result if apply

  //std::ofstream log_stream;

  common(
    int argc,
    char const* argv[])
  :
    fem::common(argc, argv)
  {}
};

  void program_main(
    const std::string& inpFile,
    const std::string& logFile,
    const std::string& outFile
    ); // int argc, char const* argv[]);
  void main10(common& cmn); // in emtp_2.cpp called from emtp_2.cpp

  void stoptp(common& cmn);
  //int locf(arr_cref<double> array);

  template<typename T>
  void move0(
    arr_ref<T>& intb,
    int const i0,
    int const n) {
    for (int i = i0, cnt = std::min(i0 + n, int(intb.size_1d())); i < cnt; ++i)
      intb(i) = 0; // 1 based
  }
  template<typename T>
  void move0(
    arr_ref<T>& intb,
    int const n) {
    move0(intb, 1, n);
  }

  template<typename T>
  void move0(
    vectorEx<T>& intb,
    int const i0,
    int const n) {
    for (int i = i0, cnt = std::min(i0 + n, int(intb.size())); i < cnt; ++i)
      intb(i) = 0; // 1 based
  }
  template<typename T>
  void move0(
    vectorEx<T>& intb,
    int const n) {
    move0(intb, 1, n);
  }

  template<typename T>
  void move0(
    ArraySpan<T>& intb,
    int const i0,
    int const n) {
    for (int i = i0, cnt = std::min(i0 + n, int(intb.size())); i < cnt; ++i)
      intb(i) = 0; // 1 based
  }
  template<typename T>
  void move0(
    ArraySpan<T>& intb,
    int const n) {
    move0(intb, 1, n);
  }


  void mover(
      arr_cref<double> a,
      arr_ref<double> b,
      int const& n);

  void dimens(
      arr_ref<int> lsize,
      int const& nchain,
      str_cref bus1,
      str_cref bus2);
  void copyi(
      int const& n1,
      arr_ref<int> ito,
      int const& kk);
  void runtym(
      double const& /* d1 */,
      double const& /* d2 */);
  void interp();
  void nextcard(common& cmn); // entry in sysdep
  void window(common& cmn);

  void cimage(common&);
  void freone(
      common& cmn,
      double& d1);
  void pfatch(
      common& cmn);
  void frefld(
      common& cmn,
      arr_ref<double> array);
  void emtspy(common&);
  void spying(common&);
  void tables(common& cmn);
  void namea6(
      common& cmn,
      str_cref text1,
      int& n24);
  void move(
      arr_cref<int> inta,
      arr_ref<int> intb,
      int const& n);
  void tacs1(common& cmn);
  template<typename T>
  int locint(
    //arr_cref<int> iarray)
    const T& iarray) 
  {
    //int return_value = fem::int0;
    ////iarray(dimension(1));
    //return_value = 0; // not applied to the testing case (loc(iarray(1))) / 4;
    return 0; // return_value;
  };
  void nmincr(
      common& cmn,
      str_ref texta,
      int const& n12);
  inline int iabsz(int const& n1) {
    return std::abs(n1);
  }
  void ibrinc(common& cmn);
  void tapsav(
      common& cmn,
      arr_ref<int> narray,
      int const& n1,
      int const& n2,
      int const& n3);
  void vecrsv(
      common& cmn,
      arr_ref<double> array,
      int const& n13,
      int const& n2);
  void matmul(
      arr_ref<double, 2> aum,
      arr_cref<double, 2> bum);
  void matvec(
      arr_cref<double, 2> aum,
      arr_ref<double> yum);
  void datain(common& cmn);
  void locatn(common& cmn);
  void mult(
      arr_cref<double> a,
      arr_cref<double> x,
      arr_ref<double> y,
      int const n,
      int const icheck);
  void honker(
      common& cmn,
      int const& klevel);
  void tekplt();
  void timval(common& cmn);
  void chrplt(common& cmn);
  void fltopt(
      common& cmn,
      double const& d8,
      int& n14);
  void csup(
      common& cmn,
      int const& L);
  void date44(
      common& cmn,
      str_arr_ref<> a);
  void time44(
      common& cmn,
      str_arr_ref<> a);
  double seedy(
      common& cmn,
      str_arr_cref<> atim);
  void cominv(
      common& cmn,
      arr_cref<double> a,
      arr_ref<double> b,
      int const& m,
      double const& freq);
  void dgelg(
      arr_ref<double> r,
      arr_ref<double> a,
      int const& m,
      int const& n,
      double const& eps,
      int& ier);
  void last14(
      common& cmn);
  void prompt(
      common& cmn);
  void identifier_switch(
      common& cmn);
  void flager(common& cmn);
  void frefp1(
      common& cmn,
      str_ref ansi,
      double& d12);
  double randnm(
      common& cmn,
      double const& x);
  void top15(
      common& cmn);
  void frefix(
      common& cmn,
      str_ref ansi,
      int const& n8);
  void tacs3(
      common& cmn);
  void packa1(
      str_cref from,
      str_ref to,
      int const& kk);
  void fildel(
    common& cmn,
    int const lun);
  void filcls(
    common& cmn,
    int const lun);



} // namespace emtp

