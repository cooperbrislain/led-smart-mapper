/**
 * Service "LEDStrip"
 */

Service.LEDStrip = function(displayName, subtype) {
  Service.call(this, displayName, '00000043-0000-1000-8000-0026BB765291', subtype);

  // Required Characteristics
  this.addCharacteristic(Characteristic.On);

  // Optional Characteristics
  this.addOptionalCharacteristic(Characteristic.Brightness);
  this.addOptionalCharacteristic(Characteristic.Hue);
  this.addOptionalCharacteristic(Characteristic.Saturation);
  this.addOptionalCharacteristic(Characteristic.Name);
  this.addOptionalCharacteristic(Characteristic.ColorTemperature); //Manual fix to add temperature
  this.addOptionalCharacteristic(Characteristic.Program); // LED Pattern Program
};

inherits(Service.Lightbulb, Service);

Service.Lightbulb.UUID = '00000043-0000-1000-8000-0026BB765291';
