#ifndef ADAPTYST_OS_DETECT_HPP_
#define ADAPTYST_OS_DETECT_HPP_

#if defined(unix) || defined(__unix) || defined(_XOPEN_SOURCE) || defined(_POSIX_SOURCE)
#define ADAPTYST_UNIX
#endif

#endif
