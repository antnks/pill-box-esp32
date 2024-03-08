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

        if (!file_exists($config))
            break;
        $event = explode("\n", file_get_contents($stamp));
        $json = json_decode(file_get_contents($config), true);

        $HOUR_GRAN = $event[0];
        $last_hour = $event[1];
        $pills = calc_pills($json["pills"]);
        $last_event = date_parse_from_format("Y-m-d\TH:i:sP", $event[2]);
        $last_val = hour_resolution(intval($last_event['hour']), intval($last_event['minute']));
        $current_hour = hour_resolution(intval(date("H")), intval(date("i")));
        $hours = calc_hours();
        $alert = intval($json["alert"]);

        if ($hours[$current_hour] != $hours[$last_val] && $current_hour == $pills[$hours[$current_hour]]+$alert)
            if(!file_exists($notify))
            {
                file_put_contents($notify, "");
                if (!isset($_GET["k"]) || !isset($_GET["i"]))
                    die();
                $key = hex2bin($_GET["k"]);
                $iv = hex2bin($_GET["i"]);
                $botApiToken = openssl_decrypt(hex2bin($encryptedToken), "aes-128-cbc", $key, OPENSSL_RAW_DATA, $iv);
                $channelId = $json["chat"];
                $text = "Pill overdue " . $json["pills"][$hours[$current_hour]];
                $query = http_build_query([
                    'chat_id' => $channelId,
                    'text' => $text,
                ]);
                $url = "https://api.telegram.org/bot{$botApiToken}/sendMessage?{$query}";
                file_get_contents($url);
            }
        else if (file_exists($notify))
            unlink($notify);
     }
}
?>
