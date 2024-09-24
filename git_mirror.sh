#!/bin/bash
# Repository mirrorer
# © John Peterson. License GNU GPL 3.

function show_usage() {
	echo mirror repo to git hub
	echo 'example usage: git_mirror.sh abc bzr http://launchpad/abc "" --debug --dry-run
--resume	resume existing mirror'
	exit 0
}

if [ $# -lt 3 ]; then
	echo too few arguments
	show_usage
	exit 1
fi

# defaults
isdebug=false
isdry=false
isresume=false
istest=false

for arg in "$@"; do
case "$arg" in
	-\?|-h|--help|--usage)
		show_usage;;
	-d|--debug)
		isdebug=true;;
	-n|--dry-run)
		isdry=true;;
	-r|--resume)
		isresume=true;;
	-t|--test)
		istest=true;;
esac
done

function mirror_exist() {
	if $isdry; then
		echo mirror_exist 0
		return 0
	fi
	echo check for existing repo
	json=$(curl -s https://api.github.com/repos/$org/$to)
	# echo $json
	error=$(echo "$json" | jshon -Q -e message -u)
	if [ -n "$error" ] && [[ "$error" != "API rate limit exceeded"* ]]; then
		echo $error;
		return 1
	else
		echo $org/$to exist
		return 0
	fi
}

# create repo
function create_repo() {
	description="$from $arg"

	if [ -z "$user" ]; then
		echo user unset
		return 1
	fi
	
	# create repo
	json=$(curl -s -u "$user:$pwd" -d "{\"name\":\"$to\"}" https://api.github.com/orgs/$org/repos)
	error=$(echo "$json" | jshon -Q -e message -u)
	if [ -n "$error" ]; then
		echo $error;
		return 1
	fi

	# set description
	json=$(curl -s -u "$user:$pwd" -X PATCH -d "{\"name\":\"$name\", \"description\":\"$description\"}" https://api.github.com/repos/$org/$name)
}

	# args
	TO=(${1//\// });
	to=${TO[0]}
	branch=${TO[1]}
	type=$2
	from=$3
	arg=$4

	# repo home
	if [ -z "$REPO_HOME" ]; then echo "REPO_HOME not set using ."; REPO_HOME=.; fi
	if [ ! -d "$REPO_HOME" ]; then echo "$REPO_HOME doesn't exist"; return 1; fi
	dir=$REPO_HOME/$to

# target org
if [ -z "$org" ]; then
	echo org is unset using mirror
	org=mirror
fi

	# create boolean
	isbzr=`if [[ "$type" =~ "bzr" ]]; then printf true; else printf false; fi`
	isgit=`if [[ "$type" =~ "git" ]]; then printf true; else printf false; fi`
	issvn=`if [[ "$type" =~ "svn" ]]; then printf true; else printf false; fi`


# debug
if $isdebug; then
	echo to $to
	echo branch $branch
	echo type $type
	echo from $from
	echo arg $arg
	set -x
fi

# disable all commands
if $isdry; then
	shopt -s expand_aliases
	for a in bzr git hg mkdir popd pushd svn; do
		alias $a="echo $a"
	done
	# type git; git -v
fi

# test
if $istest; then
	mirror_exist
	exit
fi

	# log
	echo pushing $to $(date -Is)
	
	# commands
	m="push --mirror"
	r='add'
	case "$type" in
	bzr) c="bzr clone $from ."
		p='bzr sync bzr/master'
		m="push --tags origin HEAD:master";;
	cvs) c="cvsimport -i -d $from $arg"
		p="$c";;
	git) c="clone --mirror $from ."
		p='remote update'
		s=set-url
		r='set-url --push';;
	hg) c="hg clone $from ."
		p='hg pull';;
	svn) c="svn clone $arg $from ."
		p="svn rebase"
			m="push";;
	*) echo "$type is unsupported"; return 1;;
	esac
	
	# branch push command
	if [ -n "$branch" ]; then
		dir=${dir}_$branch
		m="push origin HEAD:$branch"
	fi


# clone
# why do we need this?
# isrepo=($isgit && -f $dir/HEAD || !$isgit && -f $dir/.git/HEAD)
if [[ -d $dir ]]; then
	echo resume existing repo
	pushd $dir

else
	echo creating new folder $to
	mkdir -p $dir
	pushd $dir

	if $isresume && ! $isgit; then
		echo $type not support resume
		exit 1

	elif mirror_exist && $isgit; then
		echo clone existing mirror 
		git clone --mirror git@github.com:$org/$to .
		echo update remote
		git remote $s origin $from
		git remote $r origin git@github.com:$org/$to

	else
		echo create new repo
		if ! create_repo; then
			exit 1
		fi
		echo clone from source
		git $c
		echo update remote
		git remote $r origin git@github.com:$org/$to
	fi
fi

	# select branch
	if $isbzr; then
		git checkout bzr/master
	fi

	# sync
	git $p
	git $m

	# return to home
	popd

	if $isdebug; then set +x; fi
