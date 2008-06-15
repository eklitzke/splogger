import time
#import splogger
import socket
import sys
import random

# This should be kept in sync with the sploggerd configuration.
# TODO: make it possible to parse the sploggerd conf here
SPLOGGER_SYSTEMS = {
	'foo': 1,
	'bar': 2
}

def get_first_mac_address():
	assert sys.platform == 'linux2' # only works on linux
	arp_line = open('/proc/net/arp').readlines()[1]
	mac = arp_line.split()[3] # mac address in : form
	return mac.replace(':', '')

class EveClient(object):

	def __init__(self, splogger_group='yelp', splogger_port=None,
			splogger_host=None, splogger_service_type=None):

		# on linux we know how to get the mac address
		if sys.platform == 'linux2':
			arp_line = open('/proc/net/arp').readlines()[1]
			mac = arp_line.split()[3] # mac address in : form
			self.macaddr = mac.replace(':', '')
		else:
			# otherwise we fake it
			self.macaddr = '012X' % random.getrandbits(48)

		self.hostname = socket.gethostname()
		self.splogger_group = splogger_group

		kwargs = {'port': splogger_port, 'host': splogger_host, 'service_type':
				service_type}
		kwargs = dict((k, v) for k, v in kwargs.iteritems() if v is not None)
		self.splog = splogger.Splogger(**kwargs)

	def generate_guid(self, cur_time):
		'''The GUID is composed of three parts to guarantee that it will be
		unique in both time and space: the MAC address of the first network
		interface (only works on Linux), the time since the epoch, and a random
		sequence of additional bits (since there may not be enough precision in
		the time).
		
		It consists of exactly 30 hexadecimal characters plus two dashes (i.e.
		the total length is always exactly 32 characters)'''

		# On some *nixes (notably linux) time.time() will return a floating
		# point number (e.g. from gettimeofday()) so we might be able to get
		# extra bits by grabbing off the milliseconds/nanoseconds from the time
		# value.
		ext_time = int(cur_time * 1000) 

		# In case the time doesn't give us enough precision
		rbits = random.getrandbits(24)

		return '%s-%012X-%06X' % (self.macaddr, ext_time, rbits)

	def send(self, system, text, as_repr=False):

		try:
			code = SPLOGGER_SYSTEMS[system]
		except KeyError:
			raise ValueError, "Refusing to send to unknown splogger system `%s'" % system

		cur_time = time.time()
		t = repr(text) if as_repr else text

		# Strictly speaking, it's not necessary to log the time, since that
		# data is already encoded into the GUID. However, we provide it here
		# because it's a relatively small overhead and because it's more
		# obvious to people unfamiliar with the inner workings of the GUID that
		# this is a time value
		line = '%s %s %s %s' % (self.generate_guid(cur_time), cur_time, self.hostname, t)

		self.splog.broadcast(code, self.splogger_group, line)
