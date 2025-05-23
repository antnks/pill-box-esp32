<?php

// cronurl 'http://server/log.php?action=cron&k=xx&i=xx'
// echo -n "xx" | openssl enc -aes-128-cbc -K xx -iv xx | xxd -p -c 1000000
$encryptedToken = "xx";

function calc_hours()
{
	global $HOUR_GRAN, $pills;
	$count = count($pills);
	$hours = array();

	for ($i=0; $i<$HOUR_GRAN; $i++)
	{
		$nearest = $HOUR_GRAN + 1;
		$nearest_idx = $count + 1;
		for($j=0; $j<$count; $j++)
		{
			$diff1 = abs($pills[$j] - $i);
			$diff2 = $HOUR_GRAN - $diff1;
			$diff = ($diff1 < $diff2)?$diff1:$diff2;
			if($diff < $nearest)
			{
				$nearest = $diff;
				$nearest_idx = $j;
			}
		}
		$hours[$i] = $nearest_idx;
	}
	return $hours;
}

function hour_resolution($h, $m)
{
	global $HOUR_GRAN;
	$hg = intval($HOUR_GRAN/24);

	return intval($h)*$hg + intval(intval($m)/(60/$hg));
}

function calc_pills($p)
{
	$pills = array();
	$j = count($p);
	for($i=0; $i<$j;$i++)
	{
		$pills[$i] = hour_resolution($p[$i], 0);
	}
	return $pills;
}

date_default_timezone_set('Europe/Vilnius');

$id = "0";
if (isset($_GET["box"]) && is_numeric($_GET["box"]))
	$id = $_GET["box"];
$config = $id . ".conf";
$stamp  = $id . ".txt";
$log    = $id . "_log.txt";

$action = "n/a";
if (isset($_GET["action"]))
	$action = $_GET["action"];

$hour = 0;
if (isset($_GET["h"]) && is_numeric($_GET["h"]))
	$hour = $_GET["h"];

$gran = 0;
if (isset($_GET["gran"]) && is_numeric($_GET["gran"]))
	$gran = $_GET["gran"];

if ($action == "open")
{
	$timestamp = new DateTime();
	file_put_contents($stamp, $gran . "\n" . $hour . "\n" . $timestamp->format('c'));
	$msg = date('Y-m-d H:i:s') . ",open\n";
	file_put_contents($log, $msg, FILE_APPEND);
}
if ($action == "config" && file_exists($config))
{
	echo file_get_contents($config);
}
if ($action == "cron")
{
	for($i=1; $i<999; $i++)
	{
		$config = $i . ".conf";
		$stamp  = $i . ".txt";
		$notify = $i . ".lock";

		if (!file_exists($config) || !file_exists($stamp))
			break;
		$event = explode("\n", file_get_contents($stamp));
		$json = json_decode(file_get_contents($config), true);

		// special hack for a single pill, we need a fake one, otherwise single pill takes all 24h
		if (count($json["pills"]) == 1)
		{
			$real_pill = $json["pills"][0];
			$fake_pill = intval($real_pill / 2);
			$fake_idx = 0;
			$real_idx = 1;
			if ($fake_pill < 6)
			{
				$fake_pill += 12;
				$fake_idx = 1;
				$real_idx = 0;
			}

			$json["pills"][$fake_idx] = $fake_pill;
			$json["pills"][$real_idx] = $real_pill;
		}

		$HOUR_GRAN = $event[0];
		$last_hour = $event[1];
		$pills = calc_pills($json["pills"]);
		$last_event = date_parse_from_format("Y-m-d\TH:i:sP", $event[2]);
		$last_val = hour_resolution(intval($last_event['hour']), intval($last_event['minute']));
		$current_hour = hour_resolution(intval(date("H")), intval(date("i")));
		$hours = calc_hours();
		$alert = intval($json["alert"]);

		clearstatcache();
		$fe = file_exists($notify)?"true":"false";
		$debug_log = $i . " " . date('Y-m-d H:i:s') . " " . $last_hour . " " . $current_hour . " " . $hours[$current_hour] . " " . $hours[$last_val] . " " . $last_val . " " . $pills[$hours[$current_hour]] . " " . $alert . " " . $fe . "\n";
		file_put_contents("debug_log.txt", $debug_log, FILE_APPEND);

		// last pill taken is not current and time is $alert after pill time - this is considered a miss
		if ($hours[$current_hour] != $hours[$last_val] && $current_hour == $pills[$hours[$current_hour]]+$alert)
		{
			clearstatcache();
			if(!file_exists($notify))
			{
				// mark this pill as processed
				$HOUR_GRAN = 48;
				$current_hour = hour_resolution(intval(date("H")), intval(date("i")));
				$timestamp = new DateTime();
				file_put_contents($stamp, $HOUR_GRAN . "\n" . $current_hour . "\n" . $timestamp->format('c'));

				// put lock file as a marker
				file_put_contents($notify, "");

				// in case single pill and this one is fake, skip notification
				if (count($json["pills"]) == 2 && $hours[$current_hour] == $fake_idx)
					continue;

				// send notification
				if (!isset($_GET["k"]) || !isset($_GET["i"]))
					die();
				$key = hex2bin($_GET["k"]);
				$iv = hex2bin($_GET["i"]);
				$botApiToken = openssl_decrypt(hex2bin($encryptedToken), "aes-128-cbc", $key, OPENSSL_RAW_DATA, $iv);
				$channelId = $json["chat"];
				$text = $json["name"] . " " . $json["pills"][$hours[$current_hour]] . "h pill overdue";
				$query = http_build_query([
					'chat_id' => $channelId,
					'text' => $text,
				]);
				$url = "https://api.telegram.org/bot{$botApiToken}/sendMessage?{$query}";
				file_get_contents($url);

				// save debug log
				$log = $i . "_log.txt";
				$msg = date('Y-m-d H:i:s') . ",miss\n";
				file_put_contents($log, $msg, FILE_APPEND);
			}
		}
		else
		{
			// delete and @ supress warning if file does not exist
			@unlink($notify);
		}
	}
}
?>
