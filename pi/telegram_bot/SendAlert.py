import telebot
from telebot.types import InlineKeyboardButton, InlineKeyboardMarkup
#from DetectionScript import Too_Many_Requests, READING_OUT_OF_RANGE
from config import API_TOKEN, CHAT_ID
Too_Many_Requests = True
READING_OUT_OF_RANGE = False
# ---- import these flags (or hook them to your sensor / detection logic) ----
# -------------------------------------------------------------------------

bot = telebot.TeleBot(token=API_TOKEN)

keyboard = InlineKeyboardMarkup(row_width=2)
keyboard.add(
    InlineKeyboardButton(text="DeployHoneyPot", callback_data="DeployHoneyPot"),
    InlineKeyboardButton(text="IgnoreAttack", callback_data="IgnoreAttack"),
)

if READING_OUT_OF_RANGE:
    bot.send_message(CHAT_ID, "Temperature is out of range! Choose your action:", reply_markup=keyboard)

if Too_Many_Requests:
    bot.send_message(CHAT_ID, "Potential hacking: too many requests in a short time! Choose your action:", reply_markup=keyboard)

if not (READING_OUT_OF_RANGE or Too_Many_Requests):
    print("No flags are set, nothing sent.")