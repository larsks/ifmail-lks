################
# Low level subs 
################

sub scalar2assoc
{
	local($i,%assoc);
	foreach $i (@_)
	{
		$assoc{$i}=1;
	}
	return %assoc;
}

sub xcode
{
	local($str,@tab,$l,$c);
	$str=shift;
	@tab=@_;
	$l=length($str);
	@str=unpack("C$l",$str);
	foreach $c (@str)
	{	
		$c=$tab[$c] if $tab[$c];
	}
	return pack("C$l",@str);
}

sub quote_meta
{
	local($c);
	foreach $c (('\|','\.','\+','\$','\?','\/','\{','\}','\(','\)','\[','\]'))
	{
		$_[0]=~s/$c/$c/g;
	}
	return $_[0];
}

sub wildmat2regexp
{
	$_[0]=~s/\*/\\S\*/g;
	$_[0]="^".&quote_meta($_[0])."\$";
}

sub sequencer
{
	local($seq_val);

	$seq_val=time();
	if ( -e $SEQUENCER )
	{
		&xopen(F,$SEQUENCER) || die;
		$seq_val=<F>;
		close(F);
	}
	&xopen(F,">".$SEQUENCER) || die;
	print F (++$seq_val)."\n";
	close(F);
	return $seq_val;
}

sub basename
{
	return $_[0]=~m|([^/]+)$|;
}

sub gmtoff
{
	local(@lt,@gt,$dh,$dm);
	@lt=localtime(0);
	@gt=gmtime(0);
	$dh=$lt[2]-$gt[2];
	$dh+=24 if $lt[3]!=$gt[3];
	$dm=$lt[1]-$gt[1];
	$dm+=60, $dh>0?$dh--:$dh++ if ($lt[2]>$gt[2] && $lt[1]<$gt[1])||($lt[2]<$gt[2] && $lt[1]>$gt[1]);
	return ($dm+100*$dh);
}

sub upcase
{
	local($s)=@_;
	$s=~tr/a-z/A-Z/;
	return $s;
}

#$TAB[0x30]=0x31;

#print &xcode("oo001234567",@TAB);

1;
