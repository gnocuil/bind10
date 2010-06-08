#!/usr/local/bin/python3.1

# Copyright (C) 2010  Internet Systems Consortium.
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SYSTEMS CONSORTIUM
# DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL
# INTERNET SYSTEMS CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT,
# INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING
# FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
# NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
# WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

"""\
This file implements the statistics collecting program.

This program collects 'counters' from 'statistics' channel.
"""

import sys; sys.path.append ('@@PYTHONPATH@@')
import os

# If B10_FROM_SOURCE is set in the environment, we use data files
# from a directory relative to that, otherwise we use the ones
# installed on the system
if "B10_FROM_SOURCE" in os.environ:
    SPECFILE_LOCATION = os.environ["B10_FROM_SOURCE"] + "/src/bin/stats/statsd.spec"
else:
    PREFIX = "/usr/local/bind10"
    DATAROOTDIR = "${prefix}/share"
    SPECFILE_LOCATION = "${datarootdir}/bind10-devel/statsd.spec".replace("${datarootdir}",  DATAROOTDIR).replace("${prefix}", PREFIX)

import time
import select
import errno
import signal
import re

import isc.cc
__version__ = "v20100602"

##############################################################
# for debugging
##############################################################
print ("Started statsd")

# TODO: start up statistics thingy

class Statistics:
    """Statistics Class."""

    def __init__(self, verbose = True):
        self.debug = 1
        self.stats = {}
        self.output_interval = 10
        self.output_path = '/tmp/stats.xml'
        self.output_generation = 100
        self.myname = 'statistics'
        self.verbose = verbose
        self.shutdown = 0
        self.wrote_time = 0

    def stats_update(self, arg):
        if self.debug:
            print ("[statsd] stats_update: "+str(arg))
        if (not ('component' in arg and 'from' in arg)):
            return 1
        self.stats[arg['component']] = arg
        if self.debug:
            print ("received from=",arg['from'], "   component=", arg['component'])
            print (str(arg))
        return 0
        #if last_recvd_time > wrote_time:
        #    if debug:
        #        print ("dump stats")
        #    dump_stats(statpath, statcount, statstotal, stats)

    def config_handler(self, new_config):
        if self.debug:
            print ("[statistics] handling new config: ")
            print (str(new_config))
        answer = isc.config.ccsession.create_answer(0)
        return answer
        # TODO

    def command_handler(self, command, args):
        if self.debug:
            print ("[statsd] got command: "+str(command))
        answer = isc.config.ccsession.create_answer(1, "command not implemented")
        if type(command) != str:
            answer = isc.config.ccsession.create_answer(1, "bad command")
        else:
            cmd = command
            if cmd == "stats_update":
                result = self.stats_update(args)
                if self.debug:
                    print ("self.stats_update returned "+str(result))
                answer = isc.config.ccsession.create_answer(result)
            elif cmd == "shutdown":
                if self.debug:
                    print ("[statsd] shutdown")
                self.shutdown = 1
                answer = isc.config.ccsession.create_answer(0)
            elif cmd == "clear_counters":
                self.stats = {}
                if self.debug:
                    print ("[statsd] clear_counters")
                answer = isc.config.ccsession.create_answer(0)
            elif cmd == "show_statistics":
                answer = isc.config.ccsession.create_answer(0, self.stats)
            elif cmd == "print_settings":
                full_config = self.ccs.get_full_config()
                if self.debug:
                    print ("[stats] Full Config:")
                    for item in full_config:
                        print (item + ": " + str(full_config[item]))
                answer = isc.config.ccsession.create_answer(0)
            else:
                answer = isc.config.ccsession.create_answer(1, "Unknown command")
            if self.debug:
                print ("answer="+str(answer))
                print (str(self)+str(command)+str( args)+str( answer))
        return answer

    def main(self):
        if self.debug:
            print  ("statsd: Statistics.main()")
        self.ccs = isc.config.ModuleCCSession(SPECFILE_LOCATION, self.config_handler, self.command_handler)
        self.ccs.start()
        if self.debug:
            print ("[statsd] ccsession started")
        while not self.shutdown:
            if self.debug:
                print ("loop")
            self.ccs.check_command()
        exit (1)

def main():
    try:
        parser = OptionParser(version = __version__)
        set_cmd_options(parser)
        (options, args) = parser.parse_args()
        statistics = Statistics()
        statistics.main()
    except KeyboardInterrupt:
        log_error("exit b10-statsd")
    except isc.cc.session.SessionError as e:
        log_error(str(e))
        log_error('Error happened! is the command channel daemon running?')
    except Exception as e:
        log_error(str(e))

if __name__ == '__main__':
    statistics = Statistics()
    statistics.main()
