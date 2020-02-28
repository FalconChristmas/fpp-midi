
<?

function returnIfExists($json, $setting) {
    if ($json == null) {
        return "";
    }
    if (array_key_exists($setting, $json)) {
        return $json[$setting];
    }
    return "";
}

function convertAndGetSettings() {
    global $settings;
        
    $cfgFile = $settings['configDirectory'] . "/plugin.fpp-midi.json";
    if (file_exists($cfgFile)) {
        $j = file_get_contents($cfgFile);
        $json = json_decode($j, true);
        return $json;
    }
    $j = "{\"ports\": [], \"events\": [] }";
    return json_decode($j, true);
}

$pluginJson = convertAndGetSettings();

exec("sudo aplaymidi -l | tail -n +2 | grep -v 'RtMidi'", $output, $return_val);
$portsAvail = Array();
foreach ($output as $value) {
    $l = preg_split('/\s\s+/', $value);
    $portsAvail[] = $l[1] . ":" . $l[2];
}
unset($output);
?>


<div id="global" class="settings">
<fieldset>
<legend>MIDI Control Config</legend>

<script>
function AddOption(value, text, current) {
    var o = "<option value='" + value + "'";
    var realVal = $('<textarea />').html(value).text();

    if (value == current || realVal == current)
        o += " selected";

    o += ">" + text + "</option>";

    return o;
}

function RemoveCondition(item) {
    if ($(item).parent().find('tr').length == 1)
        return;

    $(item).remove();
}

function AddCondition(row, condition, compare, text) {
    var rows = $(row).find('.conditions > tr').length;
    var c = "<tr>";

    if (rows == 0)
        c += "<td><a href='#' class='addButton' onClick='AddCondition($(this).parent().parent().parent().parent(), \"ALWAYS\", \"\", \"\");'></a></td>";
    else
        c += "<td><a href='#' class='deleteButton' onClick='RemoveCondition($(this).parent().parent());'></a></td>";

    c += "<td><select class='conditionSelect'>";
    c += AddOption('b1', 'Byte 1', condition);
    c += AddOption('b2', 'Byte 2', condition);
    c += AddOption('b3', 'Byte 3', condition);
    c += AddOption('b4', 'Byte 4', condition);
    c += AddOption('b5', 'Byte 5', condition);
    c += "</select>";

    c += "<select class='conditionTypeSelect'>";
    c += AddOption('=', '=', compare);
    c += AddOption('!=', '!=', compare);
    c += AddOption('&lt;', '&lt;', compare);
    c += AddOption('&lt;=', '&lt;=', compare);
    c += AddOption('&gt;', '&gt;', compare);
    c += AddOption('&gt;=', '&gt;=', compare);
    c += "</select>";

    c += "<input type='text' size='18' maxlength='30' class='conditionText' value='" + text + "'>";

    c += "</td></tr>";

    $(row).find('.conditions').append(c);
}

var uniqueId = 1;
function AddMIDI() {
    var id = $("#midiEventTableBody > tr").length + 1;
    
    var html = "<tr class='fppTableRow";
    if (id % 2 != 0) {
        html += " oddRow'";
    }
    html += "'><td class='colNumber rowNumber'>" + id + ".<td><input type='text' size='50' maxlength='50' class='desc'><span style='display: none;' class='uniqueId'>" + uniqueId + "</span></td>";
    html += "<td><table><tbody class='conditions'></tbody></table>";
    html += "</td><td><table class='fppTable' border=0 id='tableMIDICommand_" + uniqueId +"'>";
    html += "<tr><td>Command:</td><td><select class='midicommand' id='midicommand" + uniqueId + "' onChange='CommandSelectChanged(\"midicommand" + uniqueId + "\", \"tableMIDICommand_" + uniqueId + "\" , false, PrintArgsInputsForEditable);'><option value=''></option></select></td></tr>";
    html += "</table></td></tr>";
    
    $("#midiEventTableBody").append(html);
    LoadCommandList($('#midicommand' + uniqueId));

    newRow = $('#midiEventTableBody > tr').last();
    $('#midiEventTableBody > tr').removeClass('selectedEntry');
    DisableButtonClass('deleteEventButton');

    uniqueId++;

    return newRow;
}

function RemoveMIDI() {
    if ($('#midiEventTableBody').find('.selectedEntry').length) {
        $('#midiEventTableBody').find('.selectedEntry').remove();
        RenumberEvents();
    }

    DisableButtonClass('deleteEventButton');
}

var midiConfig = <? echo json_encode($pluginJson, JSON_PRETTY_PRINT); ?>;
function SaveMIDIConfig(config) {
    var data = JSON.stringify(config);
    $.ajax({
        type: "POST",
        url: 'fppjson.php?command=setPluginJSON&plugin=fpp-midi',
        dataType: 'json',
        async: false,
        data: data,
        processData: false,
        contentType: 'application/json',
        success: function (data) {
        }
    });
}

function SaveEvent(row) {
    var desc = $(row).find('.desc').val();
    var conditions = [];

    $(row).find('.conditions > tr').each(function() {
        var cond     = $(this).find('.conditionSelect').val();
        var condType = $(this).find('.conditionTypeSelect').val();
        var condText = $(this).find('.conditionText').val();

        var condition = {};
        condition.condition = cond;
        condition.conditionCompare = condType;
        condition.conditionText = condText;
        conditions.push(condition);
    });

    var id = $(row).find('.uniqueId').html();
    
    var json = {
        "description": desc,
        "conditions": conditions
    };
    CommandToJSON('midicommand' + id, 'tableMIDICommand_' + id, json, true);
    return json;
}

function SavePort(row) {
    var enabled = $(row).find('.enabled').is(':checked');
    var sysEx = $(row).find('.sysEx').is(':checked');
    var timeCode = $(row).find('.timeCode').is(':checked');
    var sense = $(row).find('.sense').is(':checked');
    var name =  $(row).find('.port').html();
    
    var json = {
        "name": name,
        "enabled": enabled,
        "enableSysEx": sysEx,
        "enableTimeCode": timeCode,
        "enableSense": sense
    };
    return json;
}
function SaveMIDI() {
    midiConfig = { "ports": [], "events": []};
    var i = 0;
    $("#midiPortTableBody > tr").each(function() {
        midiConfig["ports"][i++] = SavePort(this);
    });
    i = 0;
    $("#midiEventTableBody > tr").each(function() {
        midiConfig["events"][i++] = SaveEvent(this);
    });
    
    SaveMIDIConfig(midiConfig);
}
function RefreshLastMessages() {
    $.get('api/plugin-apis/MIDI/Last', function (data) {
          $("#lastMessages").text(data);
        }
    );
}

function RenumberEvents() {
    var id = 1;
    $('#midiEventTableBody > tr').each(function() {
        $(this).find('.rowNumber').html('' + id++ + '.');
        $(this).removeClass('oddRow');

        if (id % 2 != 0) {
            $(this).addClass('oddRow');
        }
    });
}

$(document).ready(function() {

    $('#midiEventTableBody').sortable({
        update: function(event, ui) {
            RenumberEvents();
        },
        item: '> tr',
        scroll: true
    }).disableSelection();

    $('#midiEventTableBody').on('mousedown', 'tr', function(event,ui){
        $('#midiEventTableBody tr').removeClass('selectedEntry');
        $(this).addClass('selectedEntry');
        EnableButtonClass('deleteEventButton');
    });

});

</script>
<div>
<span style="float:right">
<table border=0>
<tr><td style='vertical-align: top;'>Last Messages:&nbsp;<input type="button" value="Refresh" class="buttons" onclick="RefreshLastMessages();"></td></tr><tr><td style='vertical-align: top;'><pre id="lastMessages" style='min-width:150px; margin:1px;min-height:300px;'></pre></td></tr>
</table>
</span>
<span>
<table border=0  class="fppTable">
<thead>
<tr class="fppTableHeader"><th>Enable</th><th>MIDI Device</th><th>SysEx</th><th>Time</th><th>Sense</th></tr>
</thead>
<tbody id='midiPortTableBody'>
<? foreach ($portsAvail as $port) { ?>
    <tr><td><input type="checkbox" class="enabled"></td><td class="port"><?= $port; ?></td><td><input type="checkbox" class="sysEx"></td><td><input type="checkbox" class="timeCode"></td><td><input type="checkbox" class="sense"></td></tr>
<? } ?>
</tbody>
<tfoot>
<tr><td colspan='5'>
        <input type="button" value="Save" class="buttons genericButton" onclick="SaveMIDI();">
        <input type="button" value="Add" class="buttons genericButton" onclick="AddCondition(AddMIDI(), 'ALWAYS', '', '');">
        <input id="delButton" type="button" value="Delete" class="deleteEventButton disableButtons genericButton" onclick="RemoveMIDI();">
    </td>
</tr>
</tfoot>
</table>
</span>
</div>

<div class='genericTableWrapper'>
<div class='genericTableContents'>
<table class="fppTable" id="midiEventTable"  width='100%'>
<thead><tr class="fppTableHeader"><th>#</th><th>Description</th><th>Conditions</th><th>Command</th></tr></thead>
<tbody id='midiEventTableBody'>
</tbody>
</table>
</div>
</div>

<script>
$.each(midiConfig["events"], function( key, val ) {
    var row = AddMIDI();
    $(row).find('.desc').val(val["description"]);

    for (var i = 0; i < val['conditions'].length; i++) {
        AddCondition(row,
            val['conditions'][i]['condition'],
            val['conditions'][i]['conditionCompare'],
            val['conditions'][i]['conditionText']);
    }
    var id = parseInt($(row).find('.uniqueId').html());
    PopulateExistingCommand(val, 'midicommand' + id, 'tableMIDICommand_' + id, false, PrintArgsInputsForEditable);
});

$.each(midiConfig["ports"], function( key, val ) {
    $("#midiPortTableBody > tr").each(function() {
        var name =  $(this).find('.port').html();
        if (name == val["name"]) {
            $(this).find('.enabled').prop('checked', val["enabled"]);
            $(this).find('.sysEx').prop('checked', val["enableSysEx"]);
            $(this).find('.timeCode').prop('checked', val["enableTimeCode"]);
            $(this).find('.sense').prop('checked', val["enableSense"]);
        }
    });
});

RefreshLastMessages();
</script>
</fieldset>
</div>
