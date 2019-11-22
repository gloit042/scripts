#!/usr/bin/perl
use strict;
use warnings;
use Time::HiRes;
use DateTime;
use Getopt::Std;
use IO::Select;

my $reporting_interval = 1.0; # seconds
my $bytes_this_interval = 0;
my $start_time = [Time::HiRes::gettimeofday()];

STDOUT->autoflush(1);
use vars qw($opt_t);
getopts('t:');
if ($opt_t)
{
  $reporting_interval = 0.0 + $opt_t
}

my $s = IO::Select->new();
$s->add(\*STDIN);

while (1) {
  my $dt = "";
  my $elapsed_seconds = "";
  if ($s->can_read(.5)) {
    if (defined($_ = <STDIN>)) {
      if (/ length (\d+):/) {
        $bytes_this_interval += $1;
      }
    }
  }

  $elapsed_seconds = Time::HiRes::tv_interval($start_time);
  if ($elapsed_seconds > $reporting_interval) {
     my $bps = $bytes_this_interval / $elapsed_seconds;
     $dt = DateTime->now;
     $dt->set_time_zone('Asia/Shanghai');
     printf "%s %10.2f Bps\n", $dt->format_cldr("yyyy-MM-dd'T'HH:mm:ssZ"),$bps;
     $start_time = [Time::HiRes::gettimeofday()];
     $bytes_this_interval = 0;
  }
}
