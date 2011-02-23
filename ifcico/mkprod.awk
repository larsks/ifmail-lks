BEGIN	{
		print "#include \"ftscprod.h\""
		print ""
		print "struct _ftscprod ftscprod[] = {"
	}
/^[^;]/	{
		if ($2 != "DROPPED")
			print "	{0x" $1 ",\"" $2 "\"},"
	}
END	{
		print "	{0xff,(char*)0L}"
		print "};"
	}
