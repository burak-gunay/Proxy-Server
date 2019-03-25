#ifndef Exception_h
#define Exception_h

#include <exception>
using namespace std;

struct InvalidHeaderException : public exception
{
    const char * what () const throw ()
    {
        return "Empty/Invalid HTTP Header";
    }
};

struct ContentLengthException : public exception
{
    const char * what () const throw ()
    {
        return "Header Content-length Error";
    }
};

struct HttpsQueryException : public exception
{
    const char * what () const throw ()
    {
        return "Can not resolve https host";
    }
};

struct BadRequestException : public exception
{
    const char * what () const throw ()
    {
        return "Bad request";
    }
};

struct ServerSideException : public exception
{
    const char * what () const throw ()
    {
        return "Server side error";
    }
};

#endif /* Exception_h */
