from config import API_TOKEN, OTA_VERSION_FILE
import telebot
from telebot.types import InlineKeyboardButton , InlineKeyboardMarkup

bot = telebot.TeleBot(token = API_TOKEN)

@bot.message_handler(commands=['start'])
def Bot_Start_Message(message):
    welcome_text = f"This bot is for Edge-Deception Gateway ypu can use it as a IOT security Engineer to get alarms about attacks and either ignore them or apply Honeypot to a specific device on the network"
    bot.send_message(message.chat.id , welcome_text)


Button1 = InlineKeyboardButton(text= "DeployHoneyPot" , callback_data= "DeployHoneyPot")
Button2 = InlineKeyboardButton(text= "IgnoreAttack" , callback_data= "IgnoreAttack")

inline_keyboard = InlineKeyboardMarkup(row_width=2)
inline_keyboard.add(Button1,Button2)

@bot.message_handler(commands=['emergency'])
def Bot_Start_Message(message):
   bot.send_message(message.chat.id ,"This is possible attack choose your action : ", reply_markup= inline_keyboard)

@bot.callback_query_handler(func=lambda call : True)
def check_button(call):
    if call.data == "DeployHoneyPot":
        bot.answer_callback_query(call.id,"OTA is Deployed")
        with open(OTA_VERSION_FILE,"w") as f:
            f.write("1.1")

    elif call.data == "IgnoreAttack":
        bot.answer_callback_query(call.id,"attack is ignored")
        with open(OTA_VERSION_FILE,"w") as f:
            f.write("1.0")


bot.polling()

# to know ip address ipconfig
# laptop ip : 192.168.XXX.X
# port : 8080
# enter http://192.168.XXX.X:8080/ to access the local server
