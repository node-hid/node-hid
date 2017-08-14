console.log('test-ci: Attempting to load node-hid library');
try {
    var HID = require('..');
} catch(err){
    console.log('test-ci: This should error in CI: '+err);
}
console.log('test-ci: Done');
