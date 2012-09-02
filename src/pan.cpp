#include "pan.h"

pan::internal::streambuf_to_android_log pan::android_sb;
std::ostream pan::lout(0);

void pan::init_log() 
{
    lout.rdbuf(&android_sb);
}
namespace pan
{
namespace internal
{
streambuf_to_android_log::streambuf_to_android_log ( std::size_t buff_sz, std::size_t put_back ) :
    put_back_ ( std::max ( put_back, size_t ( 1 ) ) ),
    buffer_ ( std::max ( buff_sz, put_back_ ) + put_back_ )
{
}
void streambuf_to_android_log::append ( int of, char* first, char* last )
{
    size_t size = std::distance ( first, last );

    //tmp_.clear();

    while ( first != last ) {

        if ( *first == '\n' ) {
            tmp_.push_back ( '\0' );
            __android_log_print ( ANDROID_LOG_INFO, "pan::log", tmp_.data() );
            tmp_.clear();
        } else {
            tmp_.push_back ( *first );
        }
        ++first;
    }

    // basically do the same once more for the overflow. TODO: get rid of the duplicate code somehow
    if ( of != 0 ) {
        if ( of == '\n' ) {
            tmp_.push_back ( '\0' );
            __android_log_print ( ANDROID_LOG_INFO, "pan::log", tmp_.data() );
            tmp_.clear();
        } else {
            tmp_.push_back ( of );
        }
    }
}
int streambuf_to_android_log::overflow ( int c )
{
    append ( c, pbase(), epptr() );

    setp ( &buffer_.front(), ( &buffer_.back() ) + 1 );

    return 1;

}
int streambuf_to_android_log::sync()
{
    append ( 0, pbase(), pptr() );
    setp ( &buffer_.front(), ( &buffer_.back() ) + 1 );
    return 0;

}
}
}
