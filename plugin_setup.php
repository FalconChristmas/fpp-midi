
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

$portsAvail = json_decode(file_get_contents("http://localhost/api/plugin-apis/MIDI/Devices"));

?>


<div id="global" class="settings">
<legend>MIDI Control Config</legend>

<script>
allowMultisyncCommands = true;

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
        c += "<td><button class='circularButton circularButton-vsm circularButton-sm circularButton-visible circularAddButton' onClick='AddCondition($(this).parent().parent().parent().parent(), \"ALWAYS\", \"\", \"\");'>Add</button></td>";
    else
        c += "<td><button class='circularButton circularButton-vsm circularButton-sm circularButton-visible circularDeleteButton' onClick='RemoveCondition($(this).parent().parent());'>Delete</button></td>";

    c += "<td><select class='conditionSelect'>";
    c += AddOption('b1', 'Byte 1', condition);
    c += AddOption('b2', 'Byte 2', condition);
    c += AddOption('b3', 'Byte 3', condition);
    c += AddOption('b4', 'Byte 4', condition);
    c += AddOption('b5', 'Byte 5', condition);
    c += AddOption('noteOn', 'Note On', condition);
    c += AddOption('noteOff', 'Note Off', condition);
    c += AddOption('velocity', 'Velocity', condition);
    c += AddOption('channel', 'Channel', condition);
    c += AddOption('control', 'Control Change', condition);
    c += AddOption('pitch', 'Pitch Change', condition);
    c += "</select>";

    c += "<select class='conditionTypeSelect'>";
    c += AddOption('=', '=', compare);
    c += AddOption('!=', '!=', compare);
    c += AddOption('&lt;', '&lt;', compare);
    c += AddOption('&lt;=', '&lt;=', compare);
    c += AddOption('&gt;', '&gt;', compare);
    c += AddOption('&gt;=', '&gt;=', compare);
    c += "</select>";

    c += "<input type='text' size='12' maxlength='30' class='conditionText' value='" + text + "'>";

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
    html += "'><td class='center' valign='middle'><div class='rowGrip'><i class='rowGripIcon fpp-icon-grip'></i></div></td>";
    html += "<td><input type='text' size='30' maxlength='50' class='desc'><span style='display: none;' class='uniqueId'>" + uniqueId + "</span></td>";
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
    }

    DisableButtonClass('deleteEventButton');
}

var midiConfig = <? echo json_encode($pluginJson, JSON_PRETTY_PRINT); ?>;
function SaveMIDIConfig(config) {
    var data = JSON.stringify(config);
    $.ajax({
        type: "POST",
        url: 'api/configfile/plugin.fpp-midi.json',
        dataType: 'json',
        async: false,
        data: data,
        processData: false,
        contentType: 'application/json',
        success: function (data) {
        }
    });
    SetRestartFlag(2);
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

$(document).ready(function() {

    $('#midiEventTableBody').sortable({
        update: function(event, ui) {
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



<div class="row">
    <div class="col-auto mr-auto">
        <div class="row">
            <div class="col-auto">
                <div class="fppTableWrapper fppTableWrapperAsTable">
                    <div class="fppTableContents" role="region">
                        <table class="fppSelectableRowTable">
                            <thead>
                                <tr>
                                    <th style="min-width:60px">Enable</th>
                                    <th style="padding-right: 15px;">MIDI Device</th>
                                    <th style="min-width:60px">SysEx</th>
                                    <th style="min-width:60px">Time</th><th>Sense</th>
                                </tr>
                            </thead>
                            <tbody id='midiPortTableBody'>
                                <? foreach ($portsAvail as $port) { ?>
                                <tr><td><input type="checkbox" class="enabled"></td>
                                    <td class="port" style="padding-right: 15px;"><?= $port; ?></td>
                                    <td><input type="checkbox" class="sysEx"></td>
                                    <td><input type="checkbox" class="timeCode"></td>
                                    <td><input type="checkbox" class="sense"></td></tr>
                                <? } ?>
                            </tbody>
                        </table>
                    </div>
                </div>
            </div>
        </div>
        <div classs="row">
            <div class="col-auto">
                <input type="button" value="Save" class="buttons genericButton" onclick="SaveMIDI();">
                <input type="button" value="Add" class="buttons genericButton" onclick="AddCondition(AddMIDI(), 'ALWAYS', '', '');">
                <input id="delButton" type="button" value="Delete" class="deleteEventButton disableButtons genericButton" onclick="RemoveMIDI();">
            </div>
        </div>
        <div class="row">
            <div class="col-auto">
                <div class='fppTableWrapper'>
                    <div class='fppTableContents'>
                        <table  class="fppSelectableRowTable" id="midiEventTable"  width='100%'>
                            <thead><tr class="fppTableHeader"><th>#</th><th>Description</th><th>Conditions</th><th>Command</th></tr></thead>
                            <tbody id='midiEventTableBody' class="ui-sortable"></tbody>
                        </table>
                    </div>
                </div>
            </div>
        </div>
    </div>
    <div class="col-auto">
        <div>
            <div class="row">
                <div class="col">
                    Last Messages:&nbsp;<input type="button" value="Refresh" class="buttons" onclick="RefreshLastMessages();">
                </div>
            </div>
            <div class="row">
                <div class="col">
                    <pre id="lastMessages" style='min-width:150px; margin:1px;min-height:300px;'></pre>
                </div>
            </div>
        </div>
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
</div>

