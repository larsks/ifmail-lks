BEGIN { first = 1; }
END {
	close("/usr/sbin/sendmail -t ache");
}
/^#! rnews [0-9]+$/ {
	if (!first)
		close("/usr/sbin/sendmail -t ache");
	first = 0;
	next;
}
{ print | "/usr/sbin/sendmail -t ache"; }
