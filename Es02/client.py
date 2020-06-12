import paho.mqtt.client as PahoMQTT
import time
import requests
import json

class MyMQTT(object):
	# praicamente da quel che ho capito sta classe è un qualcosa di più specifico rispetto ad un publisher
	# creato un po meglio
	def __init__(self, clientID, broker, port, notifier, clean_session=False):
		self.broker = broker
		self.port = port
		self.notifier = notifier
		self.clientID = clientID
		self._topic = ""
		self._isSubscriber = False
		# create an instance of paho.mqtt.client
		self._paho_mqtt = PahoMQTT.Client(clientID, False)
		# register the callback
		self._paho_mqtt.on_connect = self.myOnConnect
		self._paho_mqtt.on_message = self.myOnMessageReceived

	def myOnConnect(self, paho_mqtt, userdata, flags, rc):
		print("Connected to %s with result code: %d" % (self.broker, rc))

	def myOnMessageReceived(self, paho_mqtt, userdata, msg):
		# A new message is received
		self.notifier.notify(msg.topic, str(msg.payload.decode("utf-8")))

	def mySubscribe(self, topic):
		# if needed, you can do some computation or error-check before subscribing
		print("subscribing to %s" % (topic))
		# subscribe for a topic
		self._paho_mqtt.subscribe(topic, 2)
		# just to remember that it works also as a subscriber
		self._isSubscriber = True
		self._topic = topic

	def start(self):
		# manage connection to broker
		self._paho_mqtt.connect(self.broker, self.port)
		self._paho_mqtt.loop_start()

	def stop(self):
		if (self._isSubscriber):
			# remember to unsuscribe if it is working also as subscriber
			self._paho_mqtt.unsubscribe(self._topic)
		self._paho_mqtt.loop_stop()
		self._paho_mqtt.disconnect()

	def myPublish(self, topic, message):
		# publichiamo un messagio relativo ad un certo topic. l'int inviato come parametro
		# indica il grado di QOS relativo a quella comunicazione
		self._paho_mqtt.publish(topic, message, 2)

class IoTPublisher():
	def __init__(self, clientID, broker, port):
		# create an instance of MyMQTT class
		self.clientID = clientID
		self.myMqttClient = MyMQTT(self.clientID, broker, port, self)

	def run(self):
		# if needed, perform some other actions befor starting the mqtt communication
		print("running %s" % (self.clientID))
		self.myMqttClient.start()

	def end(self):
		# if needed, perform some other actions befor ending the software
		print("ending %s" % (self.clientID))
		self.myMqttClient.stop()

	def notify(self, topic, msg):
		# manage here your received message. You can perform some error-check here
		print("received '%s' under topic '%s'" % (msg, topic))

	def mySubscribe(self, topic: str):
		self.myMqttClient.mySubscribe(topic)
		print("Mi sono sottoscritto al topic : {}".format(topic))

	def mySecondPublish(self, topic, msg):
		self.myMqttClient.myPublish(topic, msg)

	def myUnsubscribe(self):
		if (self.myMqttClient._isSubscriber):
			# remember to unsuscribe if it is working also as subscriber
		  self.myMqttClient._paho_mqtt.unsubscribe(self.myMqttClient._topic)

##
# Main
##
if __name__ == '__main__':
  server = "http://localhost:8080"

  # Register as a new service
  payload = {'description': 'prova','rest':'','mqtt':''}
  r = requests.post('http://localhost:8080/services/new', data=payload)
  serviceID = int(r.text)
  print(f"serviceID: {serviceID}")

  # Get message broker info from Catalog
  r = requests.get(f"{server}/messagebroker")
  mb = json.loads(r.text)
  broker = mb.get('url')
  port = mb.get('port')
  topic = mb.get('catalogTopic')
  subtopic = ""

  # Get Yun endpoint
  r = requests.get(f"{server}/devices")
  devices = json.loads(r.text)
  end_point = devices['devices'][0]['mqtt']

  client = IoTPublisher("Lab3-1", broker, port)
  client.run()
  
  topic = topic + end_point

  # Subscribe client to topic
  client.mySubscribe(f"{topic}")

  client.end()