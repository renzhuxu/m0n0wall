<?php
/*
	$Id$
	part of m0n0wall (http://m0n0.ch/wall)
	
	Copyright (C) 2003-2007 Manuel Kasper <mk@neon1.net>.
	All rights reserved.
	
	Redistribution and use in source and binary forms, with or without
	modification, are permitted provided that the following conditions are met:
	
	1. Redistributions of source code must retain the above copyright notice,
	   this list of conditions and the following disclaimer.
	
	2. Redistributions in binary form must reproduce the above copyright
	   notice, this list of conditions and the following disclaimer in the
	   documentation and/or other materials provided with the distribution.
	
	THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
	INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
	AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
	AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
	OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
	SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
	INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
	CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
	ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
	POSSIBILITY OF SUCH DAMAGE.
*/

/* include all configuration functions */
require_once("functions.inc");

function interfaces_loopback_configure() {
	global $config, $g;

	mwexec("/sbin/ifconfig lo0 127.0.0.1");
	
	return 0;
}

function interfaces_vlan_configure() {
	global $config, $g;
	
	if (is_array($config['vlans']['vlan']) && count($config['vlans']['vlan'])) {
		
		$i = 0;
		
		foreach ($config['vlans']['vlan'] as $vlan) {
			
			$cmd = "/sbin/ifconfig vlan{$i} create vlan " . 
				escapeshellarg($vlan['tag']) . " vlandev " . 
				escapeshellarg($vlan['if']);
			
			mwexec($cmd);
			
			/* make sure the parent interface is up */
			mwexec("/sbin/ifconfig " . escapeshellarg($vlan['if']) . " up");
			
			$i++;
		}
	}
	
	return 0;
}

function interfaces_lan_configure() {
	global $config, $g;
	
	if ($g['booting'])
		echo "Configuring LAN interface... ";
	
	$lancfg = $config['interfaces']['lan'];
		
	/* wireless configuration? */
	if (is_array($lancfg['wireless']))
		interfaces_wireless_configure($lancfg['if'], $lancfg['wireless']);
	
	/* MAC spoofing? */
	if ($lancfg['spoofmac'])
		mwexec("/sbin/ifconfig " . escapeshellarg($lancfg['if']) . 
			" link " . escapeshellarg($lancfg['spoofmac']));
	
	/* media */
	if ($lancfg['media'] || $lancfg['mediaopt']) {
		$cmd = "/sbin/ifconfig " . escapeshellarg($lancfg['if']);
		if ($lancfg['media'])
			$cmd .= " media " . escapeshellarg($lancfg['media']);
		if ($lancfg['mediaopt'])
			$cmd .= " mediaopt " . escapeshellarg($lancfg['mediaopt']);
		mwexec($cmd);
	}
	
	$addflags = "";
	if (strpos($lancfg['if'], "fxp") !== false)
		$addflags .= " link0";

	if (isset($config['system']['polling']))
		$addflags .= " polling";
	
	mwexec("/sbin/ifconfig " . escapeshellarg($lancfg['if']) . " " . 
		escapeshellarg($lancfg['ipaddr'] . "/" . $lancfg['subnet']) . $addflags);
	
	if (!$g['booting']) {
		/* make new hosts file */
		system_hosts_generate();
		
		/* reconfigure static routes (kernel may have deleted them) */
		system_routing_configure();
		
		/* reload ipfilter (address may have changed) */
		filter_configure();
		
		/* reload shaper (subnet may have changed) */
		shaper_configure();
		
		/* reload IPsec tunnels */
		vpn_ipsec_configure();
		
		/* reload dhcpd (gateway may have changed) */
		services_dhcpd_configure();
		
		/* reload dnsmasq */
		services_dnsmasq_configure();
		
		/* reload webgui */
		system_webgui_start();
		
		/* reload captive portal */
		captiveportal_configure();
	}
	
	if ($g['booting'])
		echo "done\n";
	
	return 0;
}

function interfaces_optional_configure() {
	global $config, $g;
	global $bridgeconfig;
	
	/* Reset bridge configuration.	Interfaces will add to it. */
	$bridgeconfig = "";
	
	for ($i = 1; isset($config['interfaces']['opt' . $i]); $i++) {
		interfaces_optional_configure_if($i);
	}
	
	if ($bridgeconfig) {
		/* Set the system bridge configuration and enable bridging. */
		mwexec("/sbin/sysctl net.link.ether.bridge.config=" . $bridgeconfig);
		
		if (isset($config['bridge']['filteringbridge']))
			mwexec("/sbin/sysctl net.link.ether.bridge.ipf=1");
		else
			mwexec("/sbin/sysctl net.link.ether.bridge.ipf=0");
		
		mwexec("/sbin/sysctl net.link.ether.bridge.enable=1");
	} else {
		mwexec("/sbin/sysctl net.link.ether.bridge.ipf=0");
		mwexec("/sbin/sysctl net.link.ether.bridge.enable=0");
	}
	
	if (!$g['booting']) {
		/* reconfigure static routes (kernel may have deleted them) */
		system_routing_configure();
		
		/* reload ipfilter (address may have changed) */
		filter_configure();
		
		/* reload shaper (address may have changed) */
		shaper_configure();
		
		/* reload IPsec tunnels */
		vpn_ipsec_configure();
		
		/* reload dhcpd (interface enabled/disabled/bridged status may have changed) */
		services_dhcpd_configure();
		
		/* restart dnsmasq */
		services_dnsmasq_configure();
	}
	
	return 0;
}

function interfaces_optional_configure_if($opti) {
	global $config, $g;
	global $bridgeconfig;
	
	$optcfg = $config['interfaces']['opt' . $opti];
	
	if ($g['booting']) {
		$optdescr = "";
		if ($optcfg['descr'])
			$optdescr = " ({$optcfg['descr']})";
		echo "Configuring OPT{$opti}{$optdescr} interface... ";
	}
	
	if (isset($optcfg['enable'])) {
		/* wireless configuration? */
		if (is_array($optcfg['wireless']))
			interfaces_wireless_configure($optcfg['if'], $optcfg['wireless']);
		
		/* MAC spoofing? */
		if ($optcfg['spoofmac'])
			mwexec("/sbin/ifconfig " . escapeshellarg($optcfg['if']) . 
				" link " . escapeshellarg($optcfg['spoofmac']));
		
		/* media */
		if ($optcfg['media'] || $optcfg['mediaopt']) {
			$cmd = "/sbin/ifconfig " . escapeshellarg($optcfg['if']);
			if ($optcfg['media'])
				$cmd .= " media " . escapeshellarg($optcfg['media']);
			if ($optcfg['mediaopt'])
				$cmd .= " mediaopt " . escapeshellarg($optcfg['mediaopt']);
			mwexec($cmd);
		}
	
		$addflags = "";
		if (strpos($optcfg['if'], "fxp") !== false)
			$addflags .= " link0";
		
		if (isset($config['system']['polling']))
			mwexec("/sbin/ifconfig " . escapeshellarg($wancfg['if']) .  " polling") ;
	
		/* bridged? */
		if ($optcfg['bridge']) {
			mwexec("/sbin/ifconfig " . escapeshellarg($optcfg['if']) . 
				" delete up");
			
			if ($bridgeconfig != "")
				$bridgeconfig .= ",";
			
			$bridgeconfig .= $optcfg['if'] . ":" . $opti . "," .
				$config['interfaces'][$optcfg['bridge']]['if'] .
				":" . $opti;
		} else {
			mwexec("/sbin/ifconfig " . escapeshellarg($optcfg['if']) . " " . 
				escapeshellarg($optcfg['ipaddr'] . "/" . $optcfg['subnet']) . $addflags);
		}
	} else {
		mwexec("/sbin/ifconfig " . escapeshellarg($optcfg['if']) . 
			" delete down");
	}
	
	if ($g['booting'])
		echo "done\n";
	
	return 0;
}

function interfaces_wireless_configure($if, $wlcfg) {
	global $config, $g;
	
	/* wireless configuration */
	
	/* the mode/standard (11a/b/g) needs to be set separately */
	if ($wlcfg['standard']) {
		mwexec("/sbin/ifconfig " . escapeshellarg($if) . " mode " .
			escapeshellarg($wlcfg['standard']));
	}
	
	$ifcargs = escapeshellarg($if) . 
		" ssid " . escapeshellarg($wlcfg['ssid']) . " channel " . 
		escapeshellarg($wlcfg['channel']) . " ";
	
	switch ($wlcfg['mode']) {
		case 'hostap':
			$ifcargs .= "-mediaopt adhoc mediaopt hostap ";
			break;
		case 'ibss':
		case 'IBSS':
			if (strstr($if, "an"))
				$ifcargs .= "mediaopt adhoc ";
			else
				$ifcargs .= "-mediaopt hostap mediaopt adhoc ";
			break;
		case 'bss':
		case 'BSS':
			if (strstr($if, "an"))
				$ifcargs .= "-mediaopt adhoc ";
			else
				$ifcargs .= "-mediaopt hostap -mediaopt adhoc ";
			break;
	}
	
	$ifcargs .= "up";
	
	mwexec("/sbin/ifconfig " . $ifcargs);
	
	/* need to set AP-specific parameters in a separate ifconfig call */
	if ($wlcfg['mode'] == "hostap") {
		$ifcargs = escapeshellarg($if);
		
		if (isset($wlcfg['hidessid']))
			$ifcargs .= " hidessid";
		else
			$ifcargs .= " -hidessid";
			
		mwexec("/sbin/ifconfig " . $ifcargs);
	}
	
	/* configure encryption (if needed) */
	interfaces_wireless_configure_encryption($if, $wlcfg);
	
	return 0;
}

function interfaces_wireless_configure_encryption($if, $wlcfg) {
	global $config, $g;
	
	/* kill any running hostapd */
	killbypid("{$g['varrun_path']}/hostapd-$if.pid");
	unlink_if_exists("{$g['varetc_path']}/hostapd-$if.conf");
	
	/* killing hostapd leaves the interface in down state - make sure it's up again */
	mwexec("/sbin/ifconfig " . escapeshellarg($if) . " up");

	if ($wlcfg['wpa']['mode'] && $wlcfg['wpa']['mode'] != "none") {

		if ($wlcfg['mode'] == "hostap") {
			/* use WPA in hostap mode */
			$hostapdcfg = generate_hostapd_config($if, $wlcfg);
			
			/* write config to temporary file */
			$fd = fopen("{$g['varetc_path']}/hostapd-$if.conf", "w");
			if (!$fd) {
				echo("Error: cannot open hostapd-$if.conf in interfaces_wireless_configure_encryption().\n");
				return 1;
			}
	
			fwrite($fd, $hostapdcfg);
			fclose($fd);
			
			/* start hostapd */
			$pid = mwexec_bg("/usr/sbin/hostapd {$g['varetc_path']}/hostapd-$if.conf");
			
			/* write hostapd PID to file for later use */
			$fd = fopen("{$g['varrun_path']}/hostapd-$if.pid", "w");
			fwrite($fd, $pid);
			fclose($fd);
		}
		
	} else {
		/* use WEP (or no encryption) */
	
		if (isset($wlcfg['wep']['enable']) && is_array($wlcfg['wep']['key'])) {
			$ifcargs = escapeshellarg($if) . " wepmode on";
			
			$i = 1;
			foreach ($wlcfg['wep']['key'] as $wepkey) {
				$ifcargs .= " wepkey " . escapeshellarg("{$i}:{$wepkey['value']}");
				if (isset($wepkey['txkey'])) {
					$ifcargs .= " weptxkey {$i}";
				}
				$i++;
			}
			mwexec("/sbin/ifconfig " . $ifcargs);
		} else {
			/* no encryption - make sure WEP is disabled */
			mwexec("/sbin/ifconfig " . escapeshellarg($if) . " wepmode off");
		}
	}
}

function interfaces_wan_configure() {
	global $config, $g;
	
	$wancfg = $config['interfaces']['wan'];
	
	if ($g['booting'])
		echo "Configuring WAN interface... ";
	else {
		/* kill dhclient */
		mwexec("/usr/bin/killall dhclient");
		
		/* kill PPPoE client (mpd) */
		killbypid("{$g['varrun_path']}/mpd.pid");
		
		/* wait for processes to die */
		sleep(2);
		
		unlink_if_exists("{$g['varetc_path']}/dhclient.conf");
		unlink_if_exists("{$g['varetc_path']}/mpd.conf");
		unlink_if_exists("{$g['varetc_path']}/mpd.links");
		unlink_if_exists("{$g['vardb_path']}/wanip");
		unlink_if_exists("{$g['varetc_path']}/nameservers.conf");
	}
	
	/* remove all addresses first */
	while (mwexec("/sbin/ifconfig " . escapeshellarg($wancfg['if']) . " -alias") == 0);
	mwexec("/sbin/ifconfig " . escapeshellarg($wancfg['if']) . " down");
		
	/* wireless configuration? */
	if (is_array($wancfg['wireless']))
		interfaces_wireless_configure($wancfg['if'], $wancfg['wireless']);
	
	if ($wancfg['spoofmac'])
		mwexec("/sbin/ifconfig " . escapeshellarg($wancfg['if']) . 
			" link " . escapeshellarg($wancfg['spoofmac']));
		
	/* media */
	if ($wancfg['media'] || $wancfg['mediaopt']) {
		$cmd = "/sbin/ifconfig " . escapeshellarg($wancfg['if']);
		if ($wancfg['media'])
			$cmd .= " media " . escapeshellarg($wancfg['media']);
		if ($wancfg['mediaopt'])
			$cmd .= " mediaopt " . escapeshellarg($wancfg['mediaopt']);
		mwexec($cmd);
	}
	
	$addflags = "";
	if (strpos($wancfg['if'], "fxp") !== false)
		$addflags .= " link0";
	
	if (isset($config['system']['polling']))
		mwexec("/sbin/ifconfig " . escapeshellarg($wancfg['if']) .  " polling") ;
	
	switch ($wancfg['ipaddr']) {
	
		case 'dhcp':
			interfaces_wan_dhcp_configure();
			break;
			
		case 'pppoe':
			interfaces_wan_pppoe_configure();
			break;
			
		case 'pptp':
			interfaces_wan_pptp_configure();
			break;
			
		case 'bigpond':
			/* just configure DHCP for now; fire up bpalogin when we've got the lease */
			interfaces_wan_dhcp_configure();
			break;
			
		default:
			if (isset($wancfg['ispointtopoint']) && $wancfg['pointtopoint']) {
				mwexec("/sbin/ifconfig " . escapeshellarg($wancfg['if']) . " " . 
					escapeshellarg($wancfg['ipaddr'] . "/" . $wancfg['subnet']) . 
					" " . escapeshellarg($wancfg['pointtopoint']) . $addflags . " up");
			} else {
				mwexec("/sbin/ifconfig " . escapeshellarg($wancfg['if']) . " " . 
					escapeshellarg($wancfg['ipaddr'] . "/" . $wancfg['subnet']) . $addflags);
			}
			
			/* install default route */
			mwexec("/sbin/route delete default");
			mwexec("/sbin/route add default " . escapeshellarg($wancfg['gateway']));
			
			/* resync ipfilter (done automatically for DHCP/PPPoE/PPTP) */
			filter_resync();
	}
	
	if (!$g['booting']) {
		/* reconfigure static routes (kernel may have deleted them) */
		system_routing_configure();
		
		/* reload ipfilter */
		filter_configure();
		
		/* reload shaper */
		shaper_configure();
		
		/* reload ipsec tunnels */
		vpn_ipsec_configure();
		
		/* restart ez-ipupdate */
		services_dyndns_configure();
		
		/* force DNS update */
		services_dnsupdate_process();
		
		/* restart dnsmasq */
		services_dnsmasq_configure();
	}
	
	if ($g['booting'])
		echo "done\n";
	
	return 0;
}

function interfaces_wan_dhcp_configure() {
	global $config, $g;
	
	$wancfg = $config['interfaces']['wan'];

	/* generate dhclient.conf */
	$fd = fopen("{$g['varetc_path']}/dhclient.conf", "w");
	if (!$fd) {
		echo("Error: cannot open dhclient.conf in interfaces_wan_dhcp_configure().\n");
		return 1;
	}
 
 	$dhclientconf = "";
 
 	if ($wancfg['dhcphostname']) {
		$dhclientconf .= <<<EOD
send dhcp-client-identifier "{$wancfg['dhcphostname']}";
interface "{$wancfg['if']}" {
	send host-name "{$wancfg['dhcphostname']}";
}

EOD;
	}

	fwrite($fd, $dhclientconf);
	fclose($fd);
	
	/* fire up dhclient - don't wait for the lease (-nw) */
	mwexec("/sbin/dhclient -nw -cf {$g['varetc_path']}/dhclient.conf -sf /sbin/dhclient-script " .
		escapeshellarg($wancfg['if']));
	
	return 0;
}

function interfaces_wan_dhcp_down() {
	mwexec("/usr/bin/killall dhclient");

	sleep(3);
}

function interfaces_wan_dhcp_up() {
	interfaces_wan_dhcp_configure();

	sleep(3);
}

function interfaces_wan_pppoe_configure() {
	global $config, $g;
	
	$wancfg = $config['interfaces']['wan'];
	$pppoecfg = $config['pppoe'];
	
	/* generate mpd.conf */
	$fd = fopen("{$g['varetc_path']}/mpd.conf", "w");
	if (!$fd) {
		echo("Error: cannot open mpd.conf in interfaces_wan_pppoe_configure().\n");
		return 1;
	}
	
	$idle = 0;
	
	if (isset($pppoecfg['ondemand'])) {
		$ondemand = "enable";
		if ($pppoecfg['timeout'])
			$idle = $pppoecfg['timeout'];
	} else {
		$ondemand = "disable";
	}
	
	$mpdconf = <<<EOD
pppoe:
	new -i ng0 pppoe pppoe
	set iface route default
	set iface {$ondemand} on-demand
	set iface idle {$idle}
	set iface up-script /usr/local/sbin/ppp-linkup
	set iface enable tcpmssfix

EOD;
	
	if (isset($pppoecfg['ondemand'])) {
		if (isset($pppoecfg['local-ip']) && isset($pppoecfg['remote-ip'])) {
			$mpdconf .= <<<EOD
	set iface addrs {$pppoecfg['local-ip']} {$pppoecfg['remote-ip']}

EOD;
		} else {
			$mpdconf .= <<<EOD
	set iface addrs 192.0.2.112 192.0.2.113

EOD;
		}
	}
	
	$mpdconf .= <<<EOD
	set bundle disable multilink
	set bundle authname "{$pppoecfg['username']}"
	set bundle password "{$pppoecfg['password']}"
	set link keep-alive 10 60
	set link max-redial 0
	set link no acfcomp protocomp
	set link disable pap chap
	set link accept chap
	set link mtu 1492
	set ipcp yes vjcomp
	set ipcp ranges 0.0.0.0/0 0.0.0.0/0

EOD;

	if (isset($config['system']['dnsallowoverride'])) {
		$mpdconf .= <<<EOD
	set ipcp enable req-pri-dns

EOD;
		if (!isset($config['pppoe']['dnsnosec'])) {
			$mpdconf .= <<<EOD
	set ipcp enable req-sec-dns

EOD;
		}
	}
	
	$mpdconf .= <<<EOD
	open iface

EOD;

	fwrite($fd, $mpdconf);
	fclose($fd);
	
	/* generate mpd.links */
	$fd = fopen("{$g['varetc_path']}/mpd.links", "w");
	if (!$fd) {
		echo("Error: cannot open mpd.links in interfaces_wan_pppoe_configure().\n");
		return 1;
	}
		
	$mpdconf = <<<EOD
pppoe:
	set link type pppoe
	set pppoe iface {$wancfg['if']}
	set pppoe service "{$pppoecfg['provider']}"
	set pppoe enable originate
	set pppoe disable incoming

EOD;

	fwrite($fd, $mpdconf);
	fclose($fd);
	
	/* fire up mpd */
	mwexec("/usr/local/sbin/mpd -b -d {$g['varetc_path']} -p {$g['varrun_path']}/mpd.pid pppoe");
	
	return 0;
}

function interfaces_wan_pppoe_down() {
	global $g;
	sigkillbypid("{$g['varrun_path']}/mpd.pid", "SIGUSR2");

	sleep(3);
}

function interfaces_wan_pppoe_up() {
	global $g;
	sigkillbypid("{$g['varrun_path']}/mpd.pid", "SIGUSR1");

	sleep(3);
}

function interfaces_wan_pptp_configure() {
	global $config, $g;
	
	$wancfg = $config['interfaces']['wan'];
	$pptpcfg = $config['pptp'];
	
	/* generate mpd.conf */
	$fd = fopen("{$g['varetc_path']}/mpd.conf", "w");
	if (!$fd) {
		echo("Error: cannot open mpd.conf in interfaces_wan_pptp_configure().\n");
		return 1;
	}
	
	$idle = 0;
	
	if (isset($pptpcfg['ondemand'])) {
		$ondemand = "enable";
		if ($pptpcfg['timeout'])
			$idle = $pptpcfg['timeout'];
	} else {
		$ondemand = "disable";
	}
		
	$mpdconf = <<<EOD
pptp:
	new -i ng0 pptp pptp
	set iface route default
	set iface {$ondemand} on-demand
	set iface idle {$idle}
	set iface up-script /usr/local/sbin/ppp-linkup

EOD;
	
	if (isset($pptpcfg['ondemand'])) {
		$mpdconf .= <<<EOD
	set iface addrs 10.0.0.1 10.0.0.2

EOD;
	}
	
	$mpdconf .= <<<EOD
	set bundle disable multilink
	set bundle authname "{$pptpcfg['username']}"
	set bundle password "{$pptpcfg['password']}"
	set link keep-alive 10 60
	set link max-redial 0
	set link no acfcomp protocomp
	set link disable pap chap
	set link accept chap
	set ipcp no vjcomp
	set ipcp ranges 0.0.0.0/0 0.0.0.0/0

EOD;
	
	if (isset($config['system']['dnsallowoverride'])) {
		$mpdconf .= <<<EOD
	set ipcp enable req-pri-dns
	set ipcp enable req-sec-dns

EOD;
	}
	
	$mpdconf .= <<<EOD
	open

EOD;

	fwrite($fd, $mpdconf);
	fclose($fd);
	
	/* generate mpd.links */
	$fd = fopen("{$g['varetc_path']}/mpd.links", "w");
	if (!$fd) {
		echo("Error: cannot open mpd.links in interfaces_wan_pptp_configure().\n");
		return 1;
	}
		
	$mpdconf = <<<EOD
pptp:
	set link type pptp
	set pptp enable originate outcall
	set pptp disable windowing
	set pptp self {$pptpcfg['local']}
	set pptp peer {$pptpcfg['remote']}

EOD;

	fwrite($fd, $mpdconf);
	fclose($fd);
	
	/* configure interface */
	mwexec("/sbin/ifconfig " . escapeshellarg($wancfg['if']) . " " . 
		escapeshellarg($pptpcfg['local'] . "/" . $pptpcfg['subnet']));
	
	/* fire up mpd */
	mwexec("/usr/local/sbin/mpd -b -d {$g['varetc_path']} -p {$g['varrun_path']}/mpd.pid pptp");
	
	return 0;
}

function interfaces_wan_pptp_down() {
	global $g;
	sigkillbypid("{$g['varrun_path']}/mpd.pid", "SIGUSR2");

	sleep(3);
}

function interfaces_wan_pptp_up() {
	global $g;
	sigkillbypid("{$g['varrun_path']}/mpd.pid", "SIGUSR1");

	sleep(3);
}

function interfaces_wan_bigpond_configure($curwanip) {
	global $config, $g;
	
	$bpcfg = $config['bigpond'];
	
	if (!$curwanip) {
		/* IP address not configured yet, exit */
		return 0;
	}
		
	/* kill bpalogin */
	killbyname("bpalogin");
	
	/* wait a moment */
	sleep(1);
	
	/* get the default domain */
	$nfd = @fopen("{$g['varetc_path']}/defaultdomain.conf", "r");
	if ($nfd) {
		$defaultdomain = trim(fgets($nfd));
		fclose($nfd);
	}
	
	/* generate bpalogin.conf */
	$fd = fopen("{$g['varetc_path']}/bpalogin.conf", "w");
	if (!$fd) {
		echo("Error: cannot open bpalogin.conf in interfaces_wan_bigpond_configure().\n");
		return 1;
	}
	
	if (!$bpcfg['authserver'])
		$bpcfg['authserver'] = "dce-server";
	if (!$bpcfg['authdomain'])
		$bpcfg['authdomain'] = $defaultdomain;
		
	$bpconf = <<<EOD
username {$bpcfg['username']}
password {$bpcfg['password']}
authserver {$bpcfg['authserver']}
authdomain {$bpcfg['authdomain']}
localport 5050

EOD;

	if ($bpcfg['minheartbeatinterval'])
		$bpconf .= "minheartbeatinterval {$bpcfg['minheartbeatinterval']}\n";

	fwrite($fd, $bpconf);
	fclose($fd);
	
	/* fire up bpalogin */
	mwexec("/usr/local/sbin/bpalogin -c {$g['varetc_path']}/bpalogin.conf");
	
	return 0;
}

function get_real_wan_interface() {
	global $config, $g;
	
	$wancfg = $config['interfaces']['wan'];
	
	$wanif = $wancfg['if'];
	if (($wancfg['ipaddr'] == "pppoe") || ($wancfg['ipaddr'] == "pptp")) {
		$wanif = $g['pppoe_interface'];
	}
	
	return $wanif;
}

function get_current_wan_address() {
	global $config, $g;
	
	$wancfg = $config['interfaces']['wan'];
	
	if (in_array($wancfg['ipaddr'], array('pppoe','dhcp','pptp','bigpond'))) {
		/* dynamic WAN IP address, find out which one */
		$wanif = get_real_wan_interface();
		
		/* get interface info with netstat */
		exec("/usr/bin/netstat -nWI " . escapeshellarg($wanif) . " -f inet", $ifinfo);
		
		if (isset($ifinfo[1])) {
			$aif = preg_split("/\s+/", $ifinfo[1]);
			$curwanip = chop($aif[3]);
			
			if ($curwanip && is_ipaddr($curwanip) && ($curwanip != "0.0.0.0"))
				return $curwanip;
		}
		
		return null;
	} else {
		/* static WAN IP address */
		return $wancfg['ipaddr'];
	}
}

function get_interface_mac($interface) {
  
        /* build interface list with netstat */
        exec("/usr/bin/netstat -I $interface -nW -f link", $linkinfo);
        array_shift($linkinfo);
        $alink = preg_split("/\s+/", $linkinfo[0]);
        $mac = chop($alink[3]);
        return $mac;
}

function generate_hostapd_config($if, $wlcfg) {
	$config = <<<EOD
interface=$if
driver=bsd
logger_syslog=-1
logger_syslog_level=2
logger_stdout=0
ssid={$wlcfg['ssid']}
wpa={$wlcfg['wpa']['version']}

EOD;
	
	if ($wlcfg['wpa']['mode'] == "psk") {
		$config .= <<<EOD
wpa_key_mgmt=WPA-PSK
wpa_passphrase={$wlcfg['wpa']['psk']}

EOD;
	} else if ($wlcfg['wpa']['mode'] == "enterprise") {
	
		$authport = 1812;
		$acctport = 1813;
		if ($wlcfg['wpa']['radius']['authport'])
			$authport = $wlcfg['wpa']['radius']['authport'];
		if ($wlcfg['wpa']['radius']['acctport'])
			$acctport = $wlcfg['wpa']['radius']['acctport'];
	
		$config .= <<<EOD
ieee8021x=1
wpa_key_mgmt=WPA-EAP
auth_server_addr={$wlcfg['wpa']['radius']['server']}
auth_server_port={$authport}
auth_server_shared_secret={$wlcfg['wpa']['radius']['secret']}
acct_server_addr={$wlcfg['wpa']['radius']['server']}
acct_server_port={$acctport}
acct_server_shared_secret={$wlcfg['wpa']['radius']['secret']}

EOD;
	}
	
	if ($wlcfg['wpa']['cipher'] == "tkip")
		$config .= "wpa_pairwise=TKIP\n";
	else if ($wlcfg['wpa']['cipher'] == "ccmp")
		$config .= "wpa_pairwise=CCMP\n";
	else
		$config .= "wpa_pairwise=TKIP CCMP\n";
	
	return $config;
}

?>
