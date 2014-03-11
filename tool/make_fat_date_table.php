const unsigned int fat_dateTable[128][12] = {
<?php
	date_default_timezone_set("UTC");

	for($year = 1980; $year < 2107; $year++) {
?>
	{
<?php
		for($month = 0; $month < 12; $month++) {
			$dateStr = "1-" . ($month + 1) . "-" . $year;
			$int = strtotime($dateStr) - 315532800;

			if($month == 0) {
				echo "\t\t";
			} else if($month == 6) {
				echo "\n\t\t";
			}

			echo sprintf('%10du', $int);

			// delimiters
			if($month != 11) {
				echo ", ";
			}
		}
?>

	},
<?php
	}
?>
};