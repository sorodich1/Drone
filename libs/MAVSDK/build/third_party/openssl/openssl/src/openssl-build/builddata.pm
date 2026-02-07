package OpenSSL::safe::installdata;

use strict;
use warnings;
use Exporter;
our @ISA = qw(Exporter);
our @EXPORT = qw(
    @PREFIX
    @libdir
    @BINDIR @BINDIR_REL_PREFIX
    @LIBDIR @LIBDIR_REL_PREFIX
    @INCLUDEDIR @INCLUDEDIR_REL_PREFIX
    @APPLINKDIR @APPLINKDIR_REL_PREFIX
    @ENGINESDIR @ENGINESDIR_REL_LIBDIR
    @MODULESDIR @MODULESDIR_REL_LIBDIR
    @PKGCONFIGDIR @PKGCONFIGDIR_REL_LIBDIR
    @CMAKECONFIGDIR @CMAKECONFIGDIR_REL_LIBDIR
    $VERSION @LDLIBS
);

our @PREFIX                     = ( '/home/pi/Drone/libs/MAVSDK/build/third_party/openssl/openssl/src/openssl-build' );
our @libdir                     = ( '/home/pi/Drone/libs/MAVSDK/build/third_party/openssl/openssl/src/openssl-build' );
our @BINDIR                     = ( '/home/pi/Drone/libs/MAVSDK/build/third_party/openssl/openssl/src/openssl-build/apps' );
our @BINDIR_REL_PREFIX          = ( 'apps' );
our @LIBDIR                     = ( '/home/pi/Drone/libs/MAVSDK/build/third_party/openssl/openssl/src/openssl-build' );
our @LIBDIR_REL_PREFIX          = ( '' );
our @INCLUDEDIR                 = ( '/home/pi/Drone/libs/MAVSDK/build/third_party/openssl/openssl/src/openssl-build/include', '/home/pi/Drone/libs/MAVSDK/build/third_party/openssl/openssl/src/openssl-build/../openssl/include' );
our @INCLUDEDIR_REL_PREFIX      = ( 'include', '../openssl/include' );
our @APPLINKDIR                 = ( '/home/pi/Drone/libs/MAVSDK/build/third_party/openssl/openssl/src/openssl-build/ms' );
our @APPLINKDIR_REL_PREFIX      = ( 'ms' );
our @ENGINESDIR                 = ( '/home/pi/Drone/libs/MAVSDK/build/third_party/openssl/openssl/src/openssl-build/engines' );
our @ENGINESDIR_REL_LIBDIR      = ( 'engines' );
our @MODULESDIR                 = ( '/home/pi/Drone/libs/MAVSDK/build/third_party/openssl/openssl/src/openssl-build/providers' );
our @MODULESDIR_REL_LIBDIR      = ( 'providers' );
our @PKGCONFIGDIR               = ( '/home/pi/Drone/libs/MAVSDK/build/third_party/openssl/openssl/src/openssl-build' );
our @PKGCONFIGDIR_REL_LIBDIR    = ( '.' );
our @CMAKECONFIGDIR             = ( '/home/pi/Drone/libs/MAVSDK/build/third_party/openssl/openssl/src/openssl-build' );
our @CMAKECONFIGDIR_REL_LIBDIR  = ( '.' );
our $VERSION                    = '3.6.0';
our @LDLIBS                     =
    # Unix and Windows use space separation, VMS uses comma separation
    $^O eq 'VMS'
    ? split(/ *, */, '-ldl -pthread ')
    : split(/ +/, '-ldl -pthread ');

1;
