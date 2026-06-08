import { useEffect, useState } from 'react';
import { 
  Box, Container, Heading, SimpleGrid, Stat, StatLabel, StatNumber, 
  Table, Thead, Tbody, Tr, Th, Td, Flex, Text, Badge, Card, CardHeader, CardBody,
  Button, IconButton, Input, Checkbox, HStack, VStack, Divider
} from '@chakra-ui/react';

export default function Home() {
  const [stats, setStats] = useState({ total_requests: 0, total_alerts: 0 });
  const [logs, setLogs] = useState([]);
  const [alerts, setAlerts] = useState([]);
  
  // Checklist State
  const [checklist, setChecklist] = useState([]);
  const [newTask, setNewTask] = useState('');

  const fetchData = async () => {
    try {
      const [statsRes, logsRes, alertsRes, checklistRes] = await Promise.all([
        fetch('http://localhost:9000/api/stats'),
        fetch('http://localhost:9000/api/logs'),
        fetch('http://localhost:9000/api/alerts'),
        fetch('http://localhost:9000/api/checklist')
      ]);
      
      setStats(await statsRes.json());
      setLogs(await logsRes.json() || []);
      setAlerts(await alertsRes.json() || []);
      setChecklist(await checklistRes.json() || []);
    } catch (err) {
      console.error("Fetch error", err);
    }
  };

  useEffect(() => {
    fetchData();
    const interval = setInterval(fetchData, 1000);
    return () => clearInterval(interval);
  }, []);

  const clearData = async (endpoint) => {
    await fetch(`http://localhost:9000/api/${endpoint}`, { method: 'DELETE' });
    fetchData();
  };

  const checklistAction = async (action, data) => {
    await fetch('http://localhost:9000/api/checklist', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ action, ...data })
    });
    fetchData();
    if (action === 'add') setNewTask('');
  };

  return (
    <Container maxW="container.xl" py={8}>
      <Flex justify="space-between" align="center" mb={8}>
        <Heading size="xl" color="gray.100">
          SIEM Command Center
        </Heading>
        <SimpleGrid columns={2} spacing={8}>
          <Stat textAlign="right">
            <StatLabel color="gray.400">Total Requests</StatLabel>
            <StatNumber color="gray.200" fontSize="3xl">{stats.total_requests}</StatNumber>
          </Stat>
          <Stat textAlign="right">
            <StatLabel color="gray.400">Threats Blocked</StatLabel>
            <StatNumber color="red.400" fontSize="3xl">{stats.total_alerts}</StatNumber>
          </Stat>
        </SimpleGrid>
      </Flex>

      <SimpleGrid columns={{ base: 1, lg: 3 }} spacing={6} mb={6}>
        <Card bg="gray.800" borderColor="gray.700" borderWidth="1px" gridColumn={{ lg: "span 2" }}>
          <CardHeader pb={0}>
            <Flex justify="space-between" align="center">
              <Heading size="md" color="gray.300">Live Traffic Stream</Heading>
              <Button size="xs" colorScheme="red" variant="outline" onClick={() => clearData('logs')}>Clear Logs</Button>
            </Flex>
          </CardHeader>
          <CardBody overflowY="auto" maxH="400px">
            <Table variant="simple" size="sm">
              <Thead position="sticky" top={0} bg="gray.900" zIndex={1}>
                <Tr>
                  <Th color="gray.400">Time</Th>
                  <Th color="gray.400">Source</Th>
                  <Th color="gray.400">Payload</Th>
                  <Th color="gray.400" textAlign="right">Action</Th>
                </Tr>
              </Thead>
              <Tbody>
                {logs.map((log, i) => (
                  <Tr key={i} _hover={{ bg: "whiteAlpha.50" }}>
                    <Td color="gray.500">{log.timestamp.split(' ')[1]}</Td>
                    <Td color="gray.300">{log.src}</Td>
                    <Td color="gray.400" fontFamily="mono" maxW="200px" isTruncated>{log.payload}</Td>
                    <Td textAlign="right">
                      <Button 
                        size="xs" 
                        colorScheme="blue" 
                        variant="ghost" 
                        onClick={() => checklistAction('add', { task: `Review payload: ${log.payload} from IP ${log.src}` })}
                      >
                        Mark
                      </Button>
                    </Td>
                  </Tr>
                ))}
              </Tbody>
            </Table>
          </CardBody>
        </Card>

        <Card bg="gray.800" borderColor="gray.700" borderWidth="1px">
          <CardHeader pb={0}>
             <Flex justify="space-between" align="center">
              <Heading size="md" color="gray.300">Security Alerts</Heading>
              <Button size="xs" colorScheme="red" variant="outline" onClick={() => clearData('alerts')}>Clear Alerts</Button>
            </Flex>
          </CardHeader>
          <CardBody overflowY="auto" maxH="400px">
            {alerts.map((alert, i) => (
              <Box key={i} p={3} mb={3} bg="red.900" borderLeftWidth="4px" borderColor="red.400" borderRadius="md">
                <Flex justify="space-between" mb={1}>
                  <Badge colorScheme="red" variant="solid">{alert.rule}</Badge>
                  <Text fontSize="xs" color="gray.300">{alert.timestamp.split(' ')[1]}</Text>
                </Flex>
                <Text fontSize="sm" fontFamily="mono" color="red.200" wordBreak="break-all">
                  {alert.payload}
                </Text>
              </Box>
            ))}
          </CardBody>
        </Card>
      </SimpleGrid>

      {/* Sysadmin Checklist Section */}
      <Card bg="gray.800" borderColor="gray.700" borderWidth="1px">
        <CardHeader pb={0}>
           <Flex justify="space-between" align="center">
            <Heading size="md" color="gray.300">SysAdmin Task Checklist</Heading>
          </Flex>
        </CardHeader>
        <CardBody>
          <Flex mb={4} gap={2}>
            <Input 
              placeholder="Add new task (e.g., Ban IP 192.168.1.100)" 
              value={newTask} 
              onChange={(e) => setNewTask(e.target.value)}
              bg="gray.900" borderColor="gray.700" color="gray.200"
              onKeyPress={(e) => e.key === 'Enter' && checklistAction('add', { task: newTask })}
            />
            <Button colorScheme="blue" onClick={() => checklistAction('add', { task: newTask })}>Add</Button>
          </Flex>
          
          <VStack align="stretch" spacing={2}>
            {checklist.length === 0 ? (
               <Text color="gray.500" fontSize="sm">No tasks assigned.</Text>
            ) : checklist.map(item => (
              <HStack key={item.id} p={2} bg="gray.700" borderRadius="md" justify="space-between">
                <Checkbox 
                  isChecked={item.is_done} 
                  onChange={() => checklistAction('toggle', { id: item.id })}
                  colorScheme="green"
                >
                  <Text color={item.is_done ? "gray.500" : "gray.200"} textDecoration={item.is_done ? "line-through" : "none"}>
                    {item.task}
                  </Text>
                </Checkbox>
                <Button size="xs" colorScheme="red" variant="ghost" onClick={() => checklistAction('delete', { id: item.id })}>
                  Delete
                </Button>
              </HStack>
            ))}
          </VStack>
        </CardBody>
      </Card>
    </Container>
  );
}
