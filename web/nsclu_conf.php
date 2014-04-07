<?php
// nsclu sliding-window pageview counter 0002
function nsclu_count( $countup=FALSE )
{
	// --- config ---
	$pvdir		= './pv/';
	$pvn		= 6;						//
	$pvws		= 60 * 30;					// PV Window Size [sec.]
	$pvttl		= $pvws * ( $pvn + 2 );		// PV TTL [sec.]
	// --------------
	$time		= time();
	$timeE		= $time + $pvws;

	// counter
	$pvc = 0;

	$fns = scandir( $pvdir, 1 );
	if( count( $fns ) > 0 )
	{
		foreach( $fns as &$fn )
		{
			$u = strpos( $fn, '_' );
			if( $u === FALSE )
				continue;
			
			// parse
			$ts = substr( $fn, 0, $u );
			$te = substr( $fn, $u + 1 );

			// remove expired file
			if( $te + $pvttl < $time )
			{
				unlink( "{$pvdir}{$fn}" );
				continue;
			}

			// search current file
			if( $ts <= $time
			&&	$time < $te
			)
			{
				// match
				$cpvfn = "{$pvdir}{$fn}";
			}
			
			// add to counter
			$pvc += filesize( "{$pvdir}{$fn}" );
			
			// limitation
			if( $pvn-- < 0 )
			{
				unlink( "{$pvdir}{$fn}" );
			}
		}
	}
	unset( $fns );

	// count-up	
	if( $countup !== FALSE )
	{
		if( $cpvfn == '' )
		{
			// create new counter file
			$cpvfn = "{$pvdir}{$time}_{$timeE}";
		}

		// count-up
		$f = fopen( $cpvfn, 'ab' );
		fwrite( $f, 'o' );
		fclose( $f );
		unset( $f );
	}
	
	return $pvc;
}
?>