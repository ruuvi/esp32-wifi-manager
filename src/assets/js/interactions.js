
// Adds a spinner logo to selected item (css animation found in effects.css)
function addSpinner($item)
{
	if ($item.length > 0) $item.html('<div class="lds-roller"><div></div><div></div><div></div><div></div><div></div><div></div><div></div><div></div></div>');
}

function removeSpinner($item)
{
	if ($item.length > 0) $item.html();
}