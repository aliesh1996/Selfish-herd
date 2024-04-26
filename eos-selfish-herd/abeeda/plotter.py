import pandas as pd
import matplotlib.pyplot as plt

# Read the data from the CSV file
data = pd.read_csv('swarmdensity.csv', header=None, names=['generations', 'SwarmDensity'])
# data4 = pd.read_csv('evaluator.csv', header=None, names=['generations', 'Evaluator'])

# Plotting the data
plt.plot(data['generations'], data['SwarmDensity'], c='b')

# plt.plot(data4['generations'], data4['Evaluator'], c='g')
plt.xlabel('generaions')
plt.ylabel('SwarmDensity')
plt.title('Plot of Data')
plt.show()
