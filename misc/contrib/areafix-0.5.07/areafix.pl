#!/usr/bin/perl

#
# $Id: areafix.pl,v 1.2 1997/04/19 13:23:31 fnet Exp fnet $
#
# $Log: areafix.pl,v $
# Revision 1.2  1997/04/19 13:23:31  fnet
# gup3w-compatible locking added
#
# Revision 1.1  1997/04/09 13:52:10  fnet
# Initial revision
#
#

$VERSION="0.5.07";

## Type _full_ path below

$CONFIGFILE="/staff/fnet/areafix/areafix/config/config";
$CONFIGFILE=$ARGV[0] if defined($ARGV[0]);

## main program :)
&areafix();

## �������� ���������� ����������
#
# $CONFIGFILE - ��� � ������ ���� ����� ������������.
#
# ����������, ��������� �� �������
# $NO_FIDO	     - ������������ �� Areas
# $SYSOP_NAME        - ��� ���������� ���������
# $ROBOT_NAME        - ��� ������ � ������� �� �������
# $ADDRESS           - ����� �������
# $UPLINK_ROBOT_NAME - ��� ������ � ������� ( ��� ����������� �������� )
# $UPLINK_ADDRESS    - ����� �������
# $UPLINK_PASSWORD   - ������ �� Area Manager � �������
# $QUEUE	     - ���� �� ������� ��������� ���������
# $ECHO2NEWS         - ���� �� ifmail's areas file
# $HELPFILE          - ���� �� ����� � ������
# $ACTIVE            - $NEWSLIB/active
# $NEWSGROUPS        - $NEWSLIB/newsgroups
# $NEWSFEEDS         - $NEWSLIB/newsfeeds
# $PASSIVE_NEWSFEEDS - ����, � ������� ���������� ���� ����� ������ ������� %PASSIVE
# $NEWSLOCK	     - $LOCKS
# $RESTFILE          - ���� �� ����� �� ������� "�����������" ��
# $MAILCOMMAND       - ���������, ���������� ������ ( |/usr/sbin/sendmail -t )
# $OUTNEWSFEEDS      - ���� �� "��������� ����� newsfeeds" -- ������
#                      ��������� � $NEWSFEEDS
# $DESCRMISSING      - "��������" ��� ���������, �� ��������� � $NEWSLIB/newsgroups
# $RELOADCOMMAND     - ����� ������� ����������� newsfeeds
# $UPLINK_PASSWORD   - ������ �� �������� �������
# $FORWARD           - ���������� �� ������� �������
# $LOG_FACILITY	     - log facility ��� syslogd (��. syslog(2))
# $LOG_LEVEL	     - ������� ����������� ����
# $XCOM_TABLE	     - ������� ���������� ������
# $SEMAPHORE	     - ������� (�� ������ ������ :)
#
## �������� ��������� �� ������ ������ 
#
# $HELP = ���� ���� � ����� ��p���
# %passwds - ��p��� �� ���� $passwds{"hostname"}
# %feeds   - ����� ����� (������) �� newsfeeds $feeds{"hostname"}
# %passive - ����� ����� � ������������� ���������.
# %description  - �������� ��. ($description{$newsgroup})
# @restricted[$i] - ������ ��, ����������� ��� ��������.
# @sites[$i] - ����� ����� (i - ������)
# @exclude{$sites[i]} - �� exclude-�����
# @patterns{$sites[i]} - ������ �����p���
# @distribs{$sites[i]} - ������ distributions
# @flags{$sites[i]} - �����
# @params{$sites[i]} - ��p����p�
# @comments{$sites[i]} - ��������p�� _��p��_ ������ ������
# $lastcomment - ��������p�� ����� ���������� �����
# %echo2ng - �p�������� ���� �� � �����p���� $echo2ns{"newsgroup_name"}
# %ng2echo - �����p��
# $active[$i] - newsgroup
#
## ��������� �� �������� ������:
#
# $USERADDR - full address (user@host)
# $USERNAME - username part
# $USERHOST - host part ( $USERADDR eq "$USERNAME@$USERHOST")
# $SUBJECT  - letter's subject
# $FROM     - From field
# $REPLYTO  - Reply-To field
#
## other:
# $NEWSFEEDSCHANGED (==1 ���� ���� �������� add/delete)

sub areafix
{
    &startup();

    unless (&shlock($SEMAPHORE,1))
    {
	if (time-(stat($SEMAPHORE))[9] > 86400)
	{
		die &log('alert',"Possibly hung areafix (PID=$oldpid)",0);
	}
   	else
	{
		&log('warning',"Overlapping with areafix[$oldpid] - exiting",1);	}
	return;
    }

    if ($NEWSLOCK && !&shlock("$NEWSLOCK/LOCK.newsfeeds",5))
    {
	return;
    }

    &read_control_files();

    &process_queue();

    &finish();

    &shunlock("$NEWSLOCK/LOCK.newsfeeds") if $NEWSLOCK;
    &shunlock($SEMAPHORE);
}

sub process_queue
{
    local(@file,$file,$rc);

    @file=<$QUEUE/*>;

    foreach $file (@file)
    {
        if ( -f $file ) 
        { 
            open(IN,$file) || (&log('err',"Cannot open queue file $file",0), next);
            $rc=&process_message();
            close(IN);
	    if ($rc)
	    {
		unlink($file) || &log('err',"Cannot unlink queue file $file",0);
	    }
	    else
	    {
		&log('warning',"Bad message header in message ".&basename($file)." - moving to $QUEUE/bad",1);
		mkdir("$QUEUE/bad",0777) || (&log('err',"Cannot make directory $QUEUE/bad",0), next) unless ( -d "$QUEUE/bad" );
		rename($file,"$QUEUE/bad/".&basename($file)) || &log('err',"Cannot move $file to $QUEUE/bad",0);
	    }
        }
    }
}

sub process_message
{
    &parse_header() || return 0;
    open(OUT,$MAILCOMMAND) || die &log('err',"Bad mailing command $MAILCOMMAND",0);
    &log('info',"Processing message from $USERADDR",1);
    print OUT "From: $ROBOT_NAME@$ADDRESS\n";
    print OUT "To: ".(defined($REPLYTO)?$REPLYTO:$USERADDR)."\n";
    print OUT "Subject: Reply from Area Manager\n\n";

    if ( &newsfeeds_check($USERHOST) )
    {
        if ( &passwd_check($USERHOST,$SUBJECT) )
        {
            $SITE=$feeds{$USERHOST};
	    $IS_FTN_LINK=$SITE=~/^(p\d+\.)?f\d+\.n\d+$/ && !$NO_FIDO;
	    $WIDE=!$IS_FTN_LINK;
            &process_message_body();
        }
        else
        {
            &write_password_flame();
        }
    }
    else
    {
        &write_address_flame();
    }
    close(OUT);
    return 1;
}

sub startup
{ 
    &read_config($CONFIGFILE) || die "Bad configuration file";
    require "syslog.pl";
    &openlog('areafix','cons,pid',$LOG_FACILITY);
    &log('debug','Firing up',3);
}

sub finish
{
    if($NEWSFEEDSCHANGED)
    {
        if ( -e $OUTNEWSFEEDS)
        {
            if ( -e $OUTNEWSFEEDS.".bak")
            {
                unlink($OUTNEWSFEEDS.".bak") || die &log('err',"Can not unlink file $OUTNEWSFEEDS.bak",0);
            }
            rename($OUTNEWSFEEDS,$OUTNEWSFEEDS.".bak") || die &log('err',"Can not rename file $OUTNEWSFEEDS to $OUTNEWSFEEDS.bak",0);
        }
        &write_newsfeeds($OUTNEWSFEEDS,$PASSIVE_NEWSFEEDS) || &log('err',"Cannot write newsfeeds",0);

        system("$RELOADCOMMAND");
        &log('debug',"Newsfeeds reloaded",4);
    }
    close(IN);
    close(FORWARD) if ($DOING_FORWARD);
    &log('debug',"Shutting down",3);
    &closelog();
}

##--------- top-level functions
sub passwd_check
{
    return (defined $passwds{$_[0]}) && (&to_upper_case($_[1]) eq &to_upper_case($passwds{$_[0]}));
}

sub newsfeeds_check
{
    return (defined $feeds{$_[0]});
}

sub to_upper_case
{
    $_[0]=~tr/[a-z]/[A-Z]/;
    return $_[0];
}

sub quote_meta
{
    local($c);
    foreach $c (('\.','\+','\$','\?','\/','\{','\}','\(','\)','\<','\>'))
    {
	$_[0]=~s/$c/$c/g;
    }
    return $_[0];
}

sub wildmat_to_regexp
{
    $_[0]=~s/\*/\\S\*/go;
    $_[0]="^".&quote_meta($_[0])."\$";
}

sub basename
{
    local($s)=@_;
    return $1 if ($s=~m|.*/([^/]+)$|);
    return $s;
}

sub log
{
    local($prio,$message,$level)=@_;
    $message=~s/%/%%/go;
    &syslog($prio,$message) if ($level<=$LOG_LEVEL);
    return($_[1]."\n");
}

sub write_password_flame
{
    print OUT "\nDear ".&torealname($USERNAME)."!\n\n";
    print OUT "You gave incorrect password for Area Manager, so,\n";
    print OUT "your AreaFix query will be refused. Please repeat\n";
    print OUT "your request with correct password or contact\n";
    print OUT "with ".&torealname($SYSOP_NAME).", our system operator.\n\n";
    print OUT "\t\t Yours, AreaFix of $ADDRESS\n";
    &log('notice',"Remote $USERADDR gave incorrect password $SUBJECT",1);
}

sub write_restricted_flame
{
    print OUT "$_[1]$_[0] -- '$_[0]' is restricted. You cannot change its status.\n";
    &log('notice',"Remote $USERADDR attempted to \'$_[1]\' restricted echo (newsgroup) $_[0]",1);
}

sub write_address_flame
{
    print OUT "\nDear ".&torealname($USERNAME)."!\n\n";
    print OUT "I'm terribly sorry, but your site is not present\n";
    print OUT "in our newsfeeds file, so, you can not access\n";
    print OUT "Area Manager on this node. Please, contact with\n";
    print OUT &torealname($SYSOP_NAME).", our system operator.\n\n";
    print OUT "\t\t Yours, AreaFix of $ADDRESS\n";
    &log('notice',"Site $USERHOST was not found in newsfeeds",1);
}

sub process_message_body
{
    &build_hashes();
    while (<IN>)
    {
        last if (/^---/o);
	next if (/^\s*$/o);
	s/^\s*(.*)\s*$/$1/;
 	if (defined($XCOM_TABLE{$_}))
	{
	    &log('debug',"Read command: $_",3);
	    &log('debug',"Translating $_ to ".$XCOM_TABLE{$_},3); 
	    $_=$XCOM_TABLE{$_};
	}
        &log('info',"Executing command: $_",2);
	next if (/^\s*\%NOP\s*$/io);
        &print_help(), next if (/^%HELP$/io);
        &print_list(), next if (/^%LIST$/io);
        &do_wide(), next if (/^%WIDE$/io);
        &print_query(), next if (/^%QUERY$/io);
        &make_passive(), next if (/^%PASSIVE$/io);
        &make_active(), next if (/^%ACTIVE$/io);
        &print_unlinked(), next if (/^%UNLINKED$/io);
	&subscribe_to_ngroup($1), next if (/^%\+(\S+)/o);
	&unsubscribe_from_ngroup($1), next if (/^%-(\S+)/o);
        $NO_FIDO?&unsubscribe_from_ngroup($1):&unsubscribe($1), next if (/^\-(\S+)/o);
        $NO_FIDO?&subscribe_to_ngroup($1):&subscribe($1), next if (/^\+(\S+)/o);
        &print_invalid($_);
    }
    $patterns{$SITE}=join(",",sort(keys(%patt_hash)));
}

## top-level commands processing

sub make_passive
{
    &log('debug',"Processing %PASSIVE operation",4);
    if ($passive{$SITE}==1)
    {
        print OUT "%PASSIVE - your newsfeeds already have passive status\n";
    }
    else
    {
        $passive{$SITE}=1;
        $NEWSFEEDSCHANGED=1;
        print OUT "%PASSIVE - your newsfeeds have passive status now\n";
    }
}

sub make_active
{
    &log('debug',"Processing %ACTIVE operation",4);
    if ($passive{$SITE}==0)
    {
        print OUT "%ACTIVE - your news feeds already have active status\n";
    }
    else
    {
        $passive{$SITE}=0;
        $NEWSFEEDSCHANGED=1;
        print OUT "%ACTIVE - your news feeds have active status now\n";
    }
}

sub build_hashes
{
    local($group);
    undef %patt_hash;
    undef %active_hash;
    foreach $group (split(",",$patterns{$SITE}))
    {
	$patt_hash{$group}=1;
    }
    foreach $group (@active)
    {
	$active_hash{$group}=1;
    }
}

sub subscribe
{
    local($echo,@echoes,$curecho,@tmp,$curgroup);
    $echo=$_[0];
    $echo=~tr/a-z/A-Z/;
    &log('debug',"Processing subscribe operation",4);
    &wildmat_to_regexp($echo);
    @tmp=grep(/$echo/,keys(%echo2ng));
    foreach $curecho (@tmp)
    {
	push(@echoes,$curecho) if defined($active_hash{$echo2ng{$curecho}});
    } 

    if (defined(@echoes))
    {
        foreach $curecho (@echoes)
        {
            $curgroup=$echo2ng{$curecho};
   
            if (&findinrestricted($curgroup) || &findinrestricted($curecho))
	    {
                &write_restricted_flame($curecho,"+");
            }
            elsif (!defined($patt_hash{$curgroup}))
            {
                $patt_hash{$curgroup}=1;
                $NEWSFEEDSCHANGED=1;
                print OUT "+$curecho -- Subscribed to $curecho\n";
                &log('debug',"Remote subsribed to $curecho",5);
            }
            else
            {
                print OUT "+$curecho -- Already subscribed to $curecho\n";
                &log('debug',"Remote already subscribed to $curecho",5);
            }
        }
    }
    else
    {
        print OUT "+$_[0] -- No such echo(es) here.";
        &log('notice',"Can not subscribe to $_[0] - no such echo",1);
        if ($FORWARD)
        {    
            print OUT " Your request will be forwarded to our uplink. Try later.\n";
            &log('notice',"This request will be forwarded to our uplink",1);
            &forward($_[0]);
        }
	else
	{
	    print OUT "\n";
	}
    }
}

sub subscribe_to_ngroup
{
    local($group,@groups,$curgroup,@tmp);
    $group=$_[0];
    $group=~tr/A-Z/a-z/;
    &log('debug',"Processing subscribe operation to newsgroup",4);
    &wildmat_to_regexp($group);
    if ($IS_FTN_LINK)
    {
        @tmp=grep(/$group/,keys(%ng2echo));
	foreach $curgroup (@tmp)
	{
	    push(@groups,$curgroup) if defined($active_hash{$curgroup});
	}
    }
    else
    {
	@groups=grep(/$group/,@active);
    }
    if (defined(@groups))
    {
        foreach $curgroup (@groups)
        {
            if (&findinrestricted($curgroup) || &findinrestricted($ng2echo{$curgroup}))
            {
                &write_restricted_flame($curgroup,"%+");
            }
            elsif (!defined($patt_hash{$curgroup}))
            {
                $patt_hash{$curgroup}=1;
                $NEWSFEEDSCHANGED=1;
                print OUT "%+$curgroup -- Subscribed to $curgroup\n";
                &log('debug',"Remote subsribed to $curgroup",5);
            }
            else
            {
                print OUT "%+$curgroup -- Already subscribed to $curgroup\n";
                &log('debug',"Remote already subscribed to $curgroup",5);
            }
        }
    }
    else
    {
        print OUT "%+$_[0] -- No such group(s) here.\n";
        &log('notice',"Can not subscribe to newsgroup $_[0]",1);
    }
}

sub torealname
{
    local($tmp)=$_[0];
    $tmp=~tr/[_ \.]/ /;
    return $tmp;
}

sub findinrestricted
{
    local($fgroup,$fcurgroup);
    $fgroup=$_[0];
    foreach $fcurgroup (keys(%restricted))
    {
        return 1 if ($fgroup=~/$fcurgroup/i && $USERHOST=~/$restricted{$fcurgroup}/);
    }
    return 0;
}

sub forward
{
    unless ($DOING_FORWARD)
    {
	open(FORWARD,$MAILCOMMAND) || (&log('err',"Bad mailing command $MAILCOMMAND",0),return);
	$SYSOP_NAME=~tr/[_ ]/./;
	print FORWARD "From: $SYSOP_NAME@$ADDRESS\n";
	print FORWARD "To: $UPLINK_ROBOT_NAME@$UPLINK_ADDRESS\n";
	print FORWARD "Subject: $UPLINK_PASSWORD\n\n";
	$DOING_FORWARD=1;
    }	
    print FORWARD "+$_[0]\n";
}

sub unsubscribe
{
    local($echo,$curecho,$curgroup,$unsubscribed);
    $echo=$_[0];
    $echo=~tr/a-z/A-Z/;
    &log('debug',"Processing unsubscribe operation",4);
    
    &wildmat_to_regexp($echo);

    foreach $curgroup (keys(%patt_hash))
    {
        $curecho=$ng2echo{$curgroup};
        if ($curecho=~/$echo/)
        {
            if (&findinrestricted($curecho) || &findinrestricted($curgroup))
            {
                &write_restricted_flame($curecho,"-");
            }
            else
            {
		delete $patt_hash{$curgroup};
                print OUT "-$curecho -- Unsubscribed\n";
                &log('debug',"Remote unsubsribed from $curecho",5);
            }
	    $unsubscribed=1;
        }
    }

    unless ($unsubscribed)
    {
        print OUT "-$_[0] -- No match in your subscription list\n";
        &log('debug',"Remote is not subsribed to $_[0]",5);
    }
    else
    {
        $NEWSFEEDSCHANGED=1;
    }
}

sub unsubscribe_from_ngroup
{
    local($group,$curgroup,$unsubscribed);
    $group=$_[0];
    $group=~tr/A-Z/a-z/;
    &log('debug',"Processing unsubscribe operation from newsgroup",4);
    
    &wildmat_to_regexp($group);

    foreach $curgroup (keys(%patt_hash))
    {
        if ($curgroup=~/$group/)
        {
            if (&findinrestricted($curgroup) || &findinrestricted($ng2echo{$curgroup}))
            {
                &write_restricted_flame($curgroup,"%-");
            }
            else
            {
		delete $patt_hash{$curgroup};
		print OUT "%-$curgroup -- Unsubscribed\n";
                &log('debug',"Remote unsubsribed from $curgroup",5);
            }
	    $unsubscribed=1;
        }
    }

    unless ($unsubscribed)
    {
        print OUT "%-$_[0] -- No match in your subscription list\n";
        &log('debug',"Remote is not subsribed to $_[0]",5);
    }
    else
    {
        $NEWSFEEDSCHANGED=1;
    }
}

sub print_list
{
    local($echo,$group,$descr,$oldfh);
    &log('debug',"Processing %LIST operation",4);
    print OUT "\%LIST\n";
    $oldfh=select(OUT);
    $~=$NO_FIDO?NO_FIDO:($WIDE?OUTWIDE:OUT);
    foreach $group (sort($IS_FTN_LINK?values(%echo2ng):@active))
    {
	if (defined($active_hash{$group}))
        {
        	$echo=$ng2echo{$group};
		$descr=defined($description{$group})?$description{$group}:$DESCRMISSING;
        	write;
	}
    }
    select($oldfh);
}

sub do_wide
{
    $WIDE=1;
    print OUT "%WIDE -- OK\n";
}

sub print_invalid
{
    print OUT "$_[0] -- invalid command\n";
}

sub print_help 
{
    print OUT "%HELP\n$HELP\n";
}

format OUTWIDE=
@<<<<<<<<<<<<<<<<<<<<<<<<< @<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< @<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
$echo,      $group,       $descr
.
;
format OUT=
@<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< @<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
$echo,           $descr
.
format NO_FIDO=
@<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< @<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
$group,				    $descr
.

sub print_query
{
    local($echo,$group,$descr,$oldfh);
    &log('debug',"Processing %QUERY operation",4);
    print OUT "\%QUERY\n";
    $oldfh=select(OUT);
    $~=$NO_FIDO?NO_FIDO:($WIDE?OUTWIDE:OUT);
    foreach $group (sort(keys(%patt_hash)))
    {
        $echo=defined ($ng2echo{$group}) ? $ng2echo{$group}: "UNTRANSLATED";
        $descr=defined( $description{$group})? $description{$group}:$DESCRMISSING;
        write ;
    }
    select($oldfh);
}

sub print_unlinked
{
    local($echo,$group,$descr,$oldfh);
    &log('debug',"Processing %UNLINKED operation",4);
    print OUT "\%UNLINKED\n";
    $oldfh=select(OUT);
    $~=$NO_FIDO?NO_FIDO:($WIDE?OUTWIDE:OUT);
    
    foreach $echo (sort(keys(%echo2ng)))
    {
	$group=$echo2ng{$echo};
        unless (defined($patt_hash{$group}))
        {
            $descr=defined($description{$group})?$description{$group}:$DESCRMISSING;
            write ;
        }
    }
    select($oldfh);
}

# ---- RFC822-header parsing
# $USERADDR - full address (user@host)
# $USERNAME - username part
# $USERHOST - host part ( $USERADDR eq "$USERNAME@$USERHOST")
# $SUBJECT  - letter's subject
# $FROM     - From field
# inputs from IN filehandle

sub parse_addr
{
    local($line)=@_;
    return $1 if ($line=~/^[^<]*<(\S+@\S+)>/o);
    return $1 if ($line=~/^(\S+@\S+)/o);
}

sub parse_header
{
    undef $REPLYTO;
    undef $USERADDR;
    undef $SUBJECT;
    while(<IN>)
    {
        last if (/^\s*$/o); ## empty string -- break
        next unless (($headline,$value)=/([^:]+):\s*(.*)\s*$/o);
        $SUBJECT=$value if ($headline=~/^Subject$/io);
        $USERADDR=&parse_addr($value) if ($headline=~/^From$/io);
	$REPLYTO=&parse_addr($value) if ($headline=~/^Reply-To$/io);
    }
    return 0 unless (defined($USERADDR) && defined($SUBJECT));
    ($USERNAME,$USERHOST)=split("@",$USERADDR);
    return 1;
}

## -------  NEWSFEEDS MANIPULATION SUBS

sub write_newsfeeds
{
    local ($site,$groupstr,$outfile,$passive_outfile);
    $outfile=$_[0];
    $passive_outfile=$_[1];
    open(outhandle,">".$outfile) || return 0;
    open(passive_outhandle,">".$passive_outfile) || return 0;
    foreach $site (@sites) # ��� ���� ������ !
    {
	# �.�. � inn �� ��������� �������� - *, ��... 
	$patterns{$site}="!*" if ($patterns{$site} eq "");
        if ($passive{$site}==0)
        {
            print outhandle $comments{$site}; # ����������� ����� ������
            print outhandle $site; # ���������� ����
            print outhandle "/".$exclude{$site} unless($exclude{$site}eq""); # ��� ��, ������� ��������
# ��� ������� ��� ������� �����, �� �� ���� �������� ����� �� ����������������.
            print outhandle "\\\n"; # ������� \ � ������� ������.
            $groupstr=$patterns{$site}; # ������� ���� ������� �������� ���,
            $groupstr=~s|,|,\\\n\t|go; # ����� ������ ���������� ���� �� ����� ������
            print outhandle "\t:$groupstr\\\n"; # � ������� �� � newsfeeds
            print outhandle ("\t/".$distribs{$site}."\\\n") unless($distribs{$site} eq ""); # � ����� ������� ���������������, ���� ��� ���� ...
            print outhandle "\t:";
            print outhandle ($flags{$site}) unless ($flags{$site} eq ""); # ... � ����� ...
            print outhandle ":";
            print outhandle $params{$site} unless ($params{$site}eq ""); # ... � �����������.
            print outhandle "\n";
        }
        else
        {
            print passive_outhandle $comments{$site}; # ����������� ����� ������
            print outhandle $comments{$site}; # ����������� ����� ������
            print passive_outhandle $site; # ���������� ����
            print outhandle $site; # ���������� ����
            print passive_outhandle "/".$exclude{$site} unless($exclude{$site}eq""); # ��� ��, ������� ��������
            print outhandle "/".$exclude{$site} unless($exclude{$site}eq""); # ��� ��, ������� ��������
# ��� ������� ��� ������� �����, �� �� ���� �������� ����� �� ����������������.
            print passive_outhandle "\\\n"; # ������� \ � ������� ������.
            print outhandle "\\\n"; # ������� \ � ������� ������.
            $groupstr=$patterns{$site}; # ������� ���� ������� �������� ���,
            $groupstr=~s|,|,\\\n\t|go; # ����� ������ ���������� ���� �� ����� ������
            print passive_outhandle "\t:$groupstr\\\n"; # � ������� �� � passive newsfeeds
            print outhandle "\t:!*\\\n"; # � ������� !* � newsfeeds
            print passive_outhandle ("\t/".$distribs{$site}."\\\n") unless($distribs{$site} eq ""); # � ����� ������� ���������������, ���� ��� ���� ...
            print passive_outhandle "\t:";
            print outhandle "\t:";
            print passive_outhandle ($flags{$site}) unless ($flags{$site} eq ""); # ... � ����� ...
            print passive_outhandle ":";
            print passive_outhandle $params{$site} unless ($params{$site}eq ""); # ... � �����������.
            print passive_outhandle "\n";
            print outhandle ($flags{$site}) unless ($flags{$site} eq ""); # ... � ����� ...
            print outhandle ":";
            print outhandle $params{$site} unless ($params{$site}eq ""); # ... � �����������.
            print outhandle "\n";
        }
    }
    print outhandle $last_comment; # � ����������� ����� ���������� �����.
    close(outhandle);
    close(passive_outhandle);
    return 1;
}

## ---------------- reading of config files

sub read_control_files
{
    &read_active($ACTIVE) || die &log('err',"Bad active file",0);
    &read_areas($ECHO2NEWS) || die &log('err',"Bad Areas file",0) unless $NO_FIDO;
    &read_help($HELPFILE) || die &log('err',"Bad help file",0);
    &read_newsgroups($NEWSGROUPS) || die &log('err',"Bad newsgroups file",0);
    &read_newsfeeds($NEWSFEEDS) || die &log('err',"Bad newsfeeds file",0);
    &read_passive_newsfeeds($PASSIVE_NEWSFEEDS) || die &log('err',"Bad passive newsfeeds file",0);
    &read_passwd($PASSWDFILE) || die &log('err',"Bad passwords file",0);
    &read_restricted($RESTFILE) || die &log('err',"Bad restricted file",0);
    $NEWSFEEDSCHANGED=0;
    $WIDE=0;
}

## ---------------- passwd.pl --------------------
# ������ passwd
# %passwds - ��p��� �� ���� $passwds{"hostname"}
# %feeds - ����� ����� (������) �� newsfeeds $feeds{"hostname"}
#

sub read_passwd {
    local($pwd_file,$host,$feed,$passwd);
    $pwd_file=$_[0];
    open(pwd_handle,$pwd_file) || die &log('err',"Cannot open $pwd_file",0);

    while(<pwd_handle>) {
	chop;
        next if (/\s+#/o);
        next unless(($host,$feed,$passwd)=/([^:]+):([^:]+):([^:]+)/o);
        $passwds{$host}=$passwd;
        $feeds{$host}=$feed;
    }
    close(pwd_handle);
    return 1;
}

## ----------- newsgrou.pl -----------------
# ������ ����� newsgroups � ������
# %description  - �������� ��. ($description{$newsgroup})

sub read_newsgroups
{
    local($ng_file);
    $ng_file=$_[0];
    open(ng_handle,$ng_file) || die &log('err',"Cannot open $ng_file",0);

    while(<ng_handle>){
        next if(/^\s*#/o);
        next unless(($group,$descr)=/^(\S+)\s+(.*)\s*$/o);
        $description{$group}=$descr;
    }
    close(ng_handle);
    return 1;
}

## ------------------ newsfeed.pl -----------------
# newsfeeds parser
# �������� �� ����  ��� ����� � newsfeeds,
# ��p��� ��� � ����� �������:
# @sites[$i] - ����� ����� (i - ������)
# @exclude{$sites[i]} - �� exclude-�����
# @patterns{$sites[i]} - ������ �����p���
# @distribs{$sites[i]} - ������ distributions
# @flags{$sites[i]} - �����
# @params{$sites[i]} - ��p����p�
# @comments{$sites[i]} - ��������p�� _��p��_ ������ ������
# $lastcomment - ��������p�� ����� ���������� �����

sub read_newsfeeds
{
    local($current_line,$temp_line,$commentstr,@newsfeeds);
    local($index,$nf_file,$site,$sitetmp);
    local($excl,$pats,$patstmp,$distr,$flgs,$param);
    $nf_file=$_[0];
    $index=0;
    $commentstr="";
    open(handle,$nf_file) || die &log('err',"Cannot open $nf_file",0);
    push(@newsfeeds,$_) while <handle>;
    close(handle); # � ������ � ������ �������� ������� ���� � newsfeeds !

input:
    while($_=shift(@newsfeeds))
    {
        next input if /^$/o; # ������ ������ � newsfeeds ������������.

## ������ ������ ������ �� newsfeeds ...

        $current_line=$_; # ���������� ������� ������.
        if($current_line=~m/^\s*#/o) # ���� ������� ������ - �����������,
# �.�. � ������ ������ ������ �� ������ - ���
# ������� ( # ), ��
        {
            $commentstr.=$current_line; # ��������� �� ���������� � �����������
            next input; # ����� �� ���� ��������� ������ !
        }

## ������� ������ - �� ����������� ...

        chop($current_line); ##MSDOS

## ������ ��������: �������� ������� newsfeeds ��� ������� site, ��-
## �����, �����������, ����� �������� ��������� ����� ( � ����� ������,
## ��� � � C, ����� \ ��� "��������" ��������������� ) � ���� ...

        while ($current_line=~/\\$/o ) # ����, ���� ������ �� ��������� ...
        {
            chop($current_line); # ������� \ � ����� ...
            chop($temp_line=shift(@newsfeeds)); # ������ ���������, ������ \n ...
            $temp_line=~s/^\s*//o; # ��������� ���������� ������� ...
            $current_line .= $temp_line; # � ������������ �� � ����, ��� ��� ���������.
        }
        $sitestr=$current_line;

## ��� ��� �� �������� ��� ���������� � �������� site � ����� ������ � ���-
## ����: <site>:<feeds>:<flags>:<parameters> ...

        ($sitetmp,$patstmp, $flgs, $param)=split(/:/,$sitestr); # ��� ������� �� �� ��� ������ ����� !
        $site=$sitetmp; # $site - ��� �����.
        $pats=$patstmp; # $pats - ��� ��������.
        $excl=""; # $excl - ��� �������� ��� ����, �� �� ���� �������� �� ���������������� ...

## ������ ������� ��� ����� �� ���������� ����, � ���, �� ���� ��������
## ����� �� ���������������� ...

        if ($sitetmp=~m|([^/]+)/([^/]+)|o)
        {
            $site=$1;
            $excl=$2;
        }

        $distr=""; # $distr - �������� �� newsgroup distribution

## � ������ ������� �������� �� ���������� �������� � �� ���������������.

        if($patstmp=~m|([^/]+)/([^/]+)|o)
        {
            $pats=$1;
            $distr=$2;
        }

	$pats="" if ($pats eq "!*");

## �� � �������� ��� �� ��������: 

        $sites[$index]=$site; # ��� ����� -> � ������ ������ ...
        $passive{$site}=0;
        $exclude{$site}=$excl; # ���������� -> � ������ ���������� ...
        $patterns{$site}=$pats; # �������� ...
        $distribs{$site}=$distr; # ��������������� ...
        $flags{$site}=$flgs; # ����� ...
        $params{$site}=$param; # ��������� ...
        $comments{$site}=$commentstr; # ����������� ...
        $index++; # �������� ����� ����� �� 1 ...
        $commentstr=""; # ������� ������ � ������������ ...
    }
    $last_comment=$commentstr; # � ����� �������� ����������� ����� ���������� ����������.
    return 1;
}

sub read_passive_newsfeeds
{
    local($current_line,$temp_line,$commentstr);
    local($nf_file,$site,$sitetmp,$index);
    local($excl,$pats,$patstmp,$distr,$flgs,$param,$counter);
    $nf_file=$_[0];
    $commentstr="";
    open(handle,$nf_file) || die &log('err',"Cannot open $nf_file",0);
input:
    while(<handle>)
    {
        next input if /^$/o;
        $current_line=$_;
        if($current_line=~m/^\s*#/o)
        {
            $commentstr.=$current_line;
            next input;
        }
        chop($current_line); ##MSDOS
        while ($current_line=~/\\$/o)
        {
            chop($current_line);
            chop($temp_line=<handle>);
            $temp_line=~s/^\s*//o;

            $current_line.=$temp_line;
        }
        $sitestr=$current_line;
        ($sitetmp,$patstmp, $flgs, $param)=split(/:/,$sitestr);
        $site=$sitetmp;
        $pats=$patstmp;
        $excl="";
        if ($sitetmp=~m|([^/]+)/([^/]+)|o)
        {
            $site=$1;
            $excl=$2;
        }
        $distr="";
        if($patstmp=~m|([^/]+)/([^/]+)|o)
        {
            $pats=$1;
            $distr=$2;
        }
	$pats="" if ($pats eq "!*");

        $passive{$site}=1;
        $exclude{$site}=$excl;
        $patterns{$site}=$pats;
        $distribs{$site}=$distr;
        $flags{$site}=$flgs;
        $params{$site}=$param;
        $comments{$site}=$commentstr;
        $commentstr="";
    }
    close(handle);
    return 1;
}

## ----------------   read help file --------------
# ������ HELP'�
# $HELP = ���� ���� � ����� ��p���
#
sub read_help
{
    local($helpf);
    $helpf=$_[0];
    open(hh,$helpf) || die &log('err',"Cannot open $helpf",0);
    $HELP="";
    $HELP.=$_ while(<hh>);
    close(hh);
    return 1;
}

## ------------ read areas file ------------------
# ���������� ��������:
# %echo2ng - �p�������� ���� �� � �����p���� $echo2ns{"newsgroup_name"}
# %ng2echo - �����p��

sub read_areas
{
    local($areas_file,$echo,$ngroup);
    $areas_file=$_[0];
    open(areas_handle,$areas_file) || die &log('err',"Cannot open $areas_file",0);

    while(<areas_handle>)
    {
        next if (/^\s*#/o);
        next unless(($echo,$ngroup)=/(\S+)\s+(\S+)/o);
	next if ($echo=~/\*/o);
        $ngroup=~tr/A-Z/a-z/;
        $echo=~tr/a-z/A-Z/;
        $echo2ng{$echo}=$ngroup;
        $ng2echo{$ngroup}=$echo;
    }
    close(areas_handle);
    return 1;
}

sub findinactive
{
    local($tmp)=$_[0];
    &quote_meta($tmp);
    return grep(/^$tmp$/,@active);
}

## --------- read active file -----------------------
# ���������� ��������:
# $active[$i]
# 
sub read_active
{
    local($active_file,$ngroup);
    $active_file=$_[0];
    open(active_handle,$active_file) || die &log('err',"Cannot open $active_file",0);
    while(<active_handle>)
    {
        next if (/^\s*#/o);
        next unless(($ngroup)=/^(\S+)/o);
        push(@active,$ngroup);
    }
    close(active_handle);
    return 1;
}

## --------- read restricted file -----------------------
# ���������� ��������:
# $restricted[$i]
sub read_restricted
{
    local($echo,$addrpatt);
    open(handle,$_[0]) || die &log('err',"Cannot open $_[0]",0);
    while(<handle>)
    {
        next if (/^\s*#/o);
        if (/^(\S+)\s+(\S+)/o)
	{
	    $echo=$1;
	    $addrpatt=$2;
	}
	elsif (/^(\S+)/o)
	{
	    $echo=$1;
	    $addrpatt='*';
	}
	else
	{
	    next;
        }
	&wildmat_to_regexp($echo);
	&wildmat_to_regexp($addrpatt);
        $restricted{$echo}=$addrpatt;
    }
    close(handle);
    return 1;
}

sub read_config
{
    local($config_file);
    $config_file=$_[0];
    open(handle,$config_file) || die "Cannot open $config_file";
    while(<handle>)
    {
        next if (/^\s*#/o);
        next unless(($keyword,$val)=/^(\S+)\s+(.*)\s*$/o);
	$NO_FIDO=($val!~/^FTN$/io) if ($keyword=~/^Afix_Mode$/io);
        $ROBOT_NAME=$val if ($keyword=~/^Robot$/io);
        $HELPFILE=$val if ($keyword=~/^HelpFile$/io);
        $NEWSFEEDS=$val if ($keyword=~/^NewsFeeds$/io);
        $PASSIVE_NEWSFEEDS=$val if ($keyword=~/^Passive_NewsFeeds$/io);
        $NEWSGROUPS=$val if ($keyword=~/^NewsGroups$/io);
        $ECHO2NEWS=$val if ($keyword=~/^Echo2News$/io);
        $ACTIVE=$val if ($keyword=~/^Active$/io);
        $RESTFILE=$val if ($keyword=~/^RestEcho$/io);
        $PASSWDFILE=$val if ($keyword=~/^PasswdFile$/io);
        $OUTNEWSFEEDS=$val if ($keyword=~/^OutNewsFeeds$/io);
        $MAILCOMMAND=$val if ($keyword=~/^MailCommand$/io);
        $DESCRMISSING=$val if ($keyword=~/^DescrMissing$/io);
        $RELOADCOMMAND=$val if ($keyword=~/^ReloadCommand$/io);
        $SYSOP_NAME=$val if ($keyword=~/^SysOp$/io);
        $ADDRESS=$val if ($keyword=~/^Address$/io);
        $UPLINK_ADDRESS=$val if ($keyword=~/^UpLink_Address$/io);
        $UPLINK_ROBOT_NAME=$val if ($keyword=~/^UpLink_Robot$/io);
        $UPLINK_PASSWORD=$val if ($keyword=~/^UpLink_Password$/io);
	$FORWARD=($val=~/^Yes$/io)?1:0 if ($keyword=~/^Forward$/io);
	$QUEUE=$val if ($keyword=~/^Queue$/io);
	$SEMAPHORE=$val if ($keyword=~/^Semaphore$/io);
	$NEWSLOCK=$val if ($keyword=~/^NewsLockDir$/io);
	$LOG_FACILITY=$val if ($keyword=~/^LogFacility$/io);
	$LOG_LEVEL=$val if ($keyword=~/^LogLevel$/io);
	$val=~/(\S+)\s*(\S+)/o,$XCOM_TABLE{$1}=$2 if ($keyword=~/^XlateCommand$/io);
    }
    $DESCRMISSING="Description missing" unless (defined($DESCRMISSING));
    return (defined($ACTIVE)
        && ((defined($ECHO2NEWS)
        && defined($UPLINK_ADDRESS)
        && defined($UPLINK_ROBOT_NAME)
        && defined($UPLINK_PASSWORD)) || $NO_FIDO)
        && defined($NEWSGROUPS)
        && defined($HELPFILE)
        && defined($PASSWDFILE)
        && defined($OUTNEWSFEEDS)
        && defined($MAILCOMMAND)
        && defined($RELOADCOMMAND)
        && defined($SYSOP_NAME)
        && defined($ADDRESS)
        && defined($PASSIVE_NEWSFEEDS)
        && defined($NEWSFEEDS)
	&& defined($SEMAPHORE)
	&& defined($LOG_FACILITY)); 
}
##--------  end of reading of config files
#
##-------- shlock/shunlock

sub shlock
{
	local($file,$try)=@_;
	local($pid,$n,$rc);

	open(F,">$file.$$") || die "Cannot open temp lock file: $!";
	print F $$;
	close F;

	$rc=1;

	until (link("$file.$$",$file))
	{
		$rc=0, last if $try && $n++>=$try;
		open(F,$file) || die "Cannot open lock file: $!";
		$pid=<F>;
		close F;
		if ($pid && !kill(0,$pid))
		{
			unlink($file) || die "Cannot unlink stale lock: $!";
			next;
		}
		sleep(1);
	}
	unlink("$file.$$") || die "Cannot unlink temp lock file: $!";
	return $rc;
}

sub shunlock
{
	unlink($_[0]) || die "Cannot unlink lock file: $!";
}

sub _POSIX_C_SOURCE {0;}
